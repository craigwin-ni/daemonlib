/*
 * daemonlib
 * Copyright (C) 2021 Matthias Bolte <matthias@tinkerforge.com>
 *
 * fifo.c: FIFO specific functions
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
 * a FIFO object provides (non-)blocking access to a thread-safe ring buffer.
 */

#include <errno.h>
#include <string.h>

#include "fifo.h"

static int fifo_writable_at_all(FIFO *fifo) {
	if (fifo->begin <= fifo->end) {
		return fifo->length - (fifo->end - fifo->begin) - 1;
	} else {
		return fifo->begin - fifo->end - 1;
	}
}

static int fifo_writable_at_once(FIFO *fifo) {
	if (fifo->begin <= fifo->end) {
		if (fifo->begin == 0) {
			return fifo->length - fifo->end - 1;
		} else {
			return fifo->length - fifo->end;
		}
	} else {
		return fifo->begin - fifo->end - 1;
	}
}

static int fifo_readable_at_all(FIFO *fifo) {
	if (fifo->begin <= fifo->end) {
		return fifo->end - fifo->begin;
	} else {
		return fifo->length - (fifo->begin - fifo->end);
	}
}

static int fifo_readable_at_once(FIFO *fifo) {
	if (fifo->begin <= fifo->end) {
		return fifo->end - fifo->begin;
	} else {
		return fifo->length - fifo->begin;
	}
}

void fifo_create(FIFO *fifo, void *buffer, int length) {
	mutex_create(&fifo->mutex);
	condition_create(&fifo->writable_condition);
	condition_create(&fifo->readable_condition);

	fifo->buffer = buffer;
	fifo->length = length;
	fifo->begin = 0;
	fifo->end = 0;
	fifo->shutdown = false;
}

void fifo_destroy(FIFO *fifo) {
	condition_destroy(&fifo->readable_condition);
	condition_destroy(&fifo->writable_condition);
	mutex_destroy(&fifo->mutex);
}

// sets errno on error, does not short-write
int fifo_write(FIFO *fifo, const void *buffer, int length, uint32_t flags) {
	bool blocking = (flags & FIFO_FLAG_NON_BLOCKING) == 0;
	int writable;
	int written = 0;

	mutex_lock(&fifo->mutex);

	if (fifo->shutdown) {
		mutex_unlock(&fifo->mutex);

		errno = EPIPE;

		return -1;
	}

	if (length <= 0) {
		mutex_unlock(&fifo->mutex);

		return 0;
	}

	if (!blocking) {
		if (length > fifo->length - 1) {
			mutex_unlock(&fifo->mutex);

			errno = E2BIG;

			return -1;
		}

		if (length > fifo_writable_at_all(fifo)) {
			mutex_unlock(&fifo->mutex);

			errno = EWOULDBLOCK;

			return -1;
		}
	}

	while (length - written > 0) {
		if (blocking) {
			while (fifo_writable_at_all(fifo) <= 0) {
				condition_wait(&fifo->writable_condition, &fifo->mutex);

				// no point in trying to write any remaining data now. depending
				// on the thread scheduling a fifo_read call in oather thread
				// might have already  returned 0 (end-of-file) between the time
				// the writable condition being signalled and this thread being
				// able to act on it. therefore, just give up here.
				if (fifo->shutdown) {
					mutex_unlock(&fifo->mutex);

					errno = EPIPE;

					return -1;
				}
			}
		}

		writable = fifo_writable_at_once(fifo);

		if (writable > length - written) {
			writable = length - written;
		}

		memcpy((uint8_t *)fifo->buffer + fifo->end, (const uint8_t *)buffer + written, writable);

		fifo->end = (fifo->end + writable) % fifo->length;
		written += writable;

		condition_broadcast(&fifo->readable_condition);
	}

	mutex_unlock(&fifo->mutex);

	return written;
}

// sets errno on error, can short-read
int fifo_read(FIFO *fifo, void *buffer, int length, uint32_t flags) {
	bool blocking = (flags & FIFO_FLAG_NON_BLOCKING) == 0;
	int readable;
	int read = 0;

	mutex_lock(&fifo->mutex);

	if (length <= 0) {
		mutex_unlock(&fifo->mutex);

		return 0;
	}

	if (fifo_readable_at_all(fifo) <= 0) {
		if (fifo->shutdown) {
			mutex_unlock(&fifo->mutex);

			return 0;
		}

		if (!blocking) {
			mutex_unlock(&fifo->mutex);

			errno = EWOULDBLOCK;

			return -1;
		}
	}

	if (blocking) {
		while (fifo_readable_at_all(fifo) <= 0) {
			condition_wait(&fifo->readable_condition, &fifo->mutex);

			if (fifo->shutdown) {
				break;
			}
		}
	}

	while (fifo_readable_at_all(fifo) > 0 && length - read > 0) {
		readable = fifo_readable_at_once(fifo);

		if (readable > length - read) {
			readable = length - read;
		}

		memcpy((uint8_t *)buffer + read, (uint8_t *)fifo->buffer + fifo->begin, readable);

		fifo->begin = (fifo->begin + readable) % fifo->length;
		read += readable;

		condition_broadcast(&fifo->writable_condition);
	}

	mutex_unlock(&fifo->mutex);

	return read;
}

void fifo_shutdown(FIFO *fifo) {
	mutex_lock(&fifo->mutex);

	fifo->shutdown = true;

	condition_broadcast(&fifo->writable_condition);
	condition_broadcast(&fifo->readable_condition);

	mutex_unlock(&fifo->mutex);
}
