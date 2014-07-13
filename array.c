/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * array.c: Array specific functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * an Array stores items in a continuous block of memory and provides random
 * access to it. when items are added/removed from the Array then other items
 * might have to be moved in memory to keep the block of memory continuous.
 * this requires that the items are relocatable in memory. if the items do not
 * have this property then the Array will allocate extra memory per item and
 * store a pointer to this extra memory in its continuous block of memory.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"

// sets errno on error
int array_create(Array *array, int reserve, int size, int relocatable) {
	reserve = GROW_ALLOCATION(reserve);

	array->allocated = 0;
	array->count = 0;
	array->size = size;
	array->relocatable = relocatable;
	array->bytes = calloc(reserve, relocatable ? size : (int)sizeof(void *));

	if (array->bytes == NULL) {
		errno = ENOMEM;

		return -1;
	}

	array->allocated = reserve;

	return 0;
}

void array_destroy(Array *array, ItemDestroyFunction destroy) {
	int i;
	void *item;

	if (destroy != NULL) {
		for (i = 0; i < array->count; ++i) {
			item = array_get(array, i);

			destroy(item);

			if (!array->relocatable) {
				free(item);
			}
		}
	} else if (!array->relocatable) {
		for (i = 0; i < array->count; ++i) {
			free(array_get(array, i));
		}
	}

	free(array->bytes);
}

// sets errno on error
int array_reserve(Array *array, int reserve) {
	int size = array->relocatable ? array->size : (int)sizeof(void *);
	uint8_t *bytes;

	if (array->allocated >= reserve) {
		return 0;
	}

	reserve = GROW_ALLOCATION(reserve);
	bytes = realloc(array->bytes, reserve * size);

	if (bytes == NULL) {
		errno = ENOMEM;

		return -1;
	}

	memset(bytes + array->allocated * size, 0, (reserve - array->allocated) * size);

	array->allocated = reserve;
	array->bytes = bytes;

	return 0;
}

// sets errno on error
int array_resize(Array *array, int count, ItemDestroyFunction destroy) {
	int rc;
	int i;
	void *item;

	if (array->count < count) { // grow
		rc = array_reserve(array, count);

		if (rc < 0) {
			return rc;
		}

		if (!array->relocatable) {
			for (i = array->count; i < count; ++i) {
				item = calloc(1, array->size);

				if (item == NULL) {
					for (--i; i >= array->count; --i) {
						free(array_get(array, i));
					}

					errno = ENOMEM;

					return -1;
				}

				*(void **)(array->bytes + sizeof(void *) * i) = item;
			}
		}
	} else if (array->count > count) { // shrink
		if (destroy != NULL) {
			for (i = count; i < array->count; ++i) {
				item = array_get(array, i);

				destroy(item);

				if (!array->relocatable) {
					free(item);
				}
			}
		} else if (!array->relocatable) {
			for (i = count; i < array->count; ++i) {
				free(array_get(array, i));
			}
		}
	}

	array->count = count;

	return 0;
}

// sets errno on error
void *array_append(Array *array) {
	void *item;

	if (array_reserve(array, array->count + 1) < 0) {
		return NULL;
	}

	if (array->relocatable) {
		item = array_get(array, array->count);
	} else {
		item = calloc(1, array->size);

		if (item == NULL) {
			errno = ENOMEM;

			return NULL;
		}

		*(void **)(array->bytes + sizeof(void *) * array->count) = item;
	}

	++array->count;

	return item;
}

void array_remove(Array *array, int i, ItemDestroyFunction destroy) {
	void *item = array_get(array, i);
	int size = array->relocatable ? array->size : (int)sizeof(void *);
	int tail;

	if (destroy != NULL) {
		destroy(item);
	}

	if (!array->relocatable) {
		free(item);
	}

	tail = (array->count - i - 1) * size;

	if (tail > 0) {
		memmove(array->bytes + size * i, array->bytes + size * (i + 1), tail);
	}

	memset(array->bytes + size * (array->count - 1), 0, size);

	--array->count;
}

void *array_get(Array *array, int i) {
	if (array->relocatable) {
		return array->bytes + array->size * i;
	} else {
		return *(void **)(array->bytes + sizeof(void *) * i);
	}
}
