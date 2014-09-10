/*
 * daemonlib
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * conf_file.c: Reads and writes .conf formatted files
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf_file.h"

static void conf_file_line_destroy(void *item) {
	ConfFileLine *line = item;

	free(line->raw);
	free(line->name);
	free(line->value);
}

// sets errno on error
static int conf_file_parse_line(ConfFile *conf_file, int number, char *buffer,
                                ConfFileReadWarningFunction warning, void *opaque) {
	char *raw;
	char *name = NULL;
	char *value = NULL;
	char *name_end;
	char *value_end;
	ConfFileLine *line;
	int saved_errno;

	// backup line, so it can be modified in-place
	raw = strdup(buffer);

	if (raw == NULL) {
		errno = ENOMEM;

		return -1;
	}

	// strip initial whitespace. the line can contain \r because only \n is used
	// as end-of-line marker. treat \r as regular whitespace
	name = buffer + strspn(buffer, " \t\r");

	// check for empty line and comment
	if (*name == '\0' || *name == '#') {
		name = NULL;
	} else {
		// split name and value
		value = strchr(name, '=');

		if (value != NULL) {
			name_end = value;
			*value++ = '\0';

			// remove trailing whitespace at end of name
			while (name_end > name && strspn(name_end - 1, " \t\r") > 0) {
				*--name_end = '\0';
			}

			if (*name == '\0') {
				name = NULL;
				value = NULL;

				if (warning != NULL) {
					warning(CONF_FILE_READ_WARNING_NAME_MISSING, number, raw, opaque);
				}
			} else if ((conf_file->flags & CONF_FILE_FLAG_TRIM_VALUE_ON_READ) != 0) {
				// strip whitespace around value, if requested
				value = value + strspn(value, " \t\r");
				value_end = value + strlen(value);

				while (value_end > value && strspn(value_end - 1, " \t\r") > 0) {
					*--value_end = '\0';
				}
			}
		} else {
			name = NULL;

			if (warning != NULL) {
				warning(CONF_FILE_READ_WARNING_EQUAL_SIGN_MISSING, number, raw, opaque);
			}
		}
	}

	// add new line
	line = array_append(&conf_file->lines);

	if (line == NULL) {
		free(raw);
		free(name);
		free(value);

		return -1;
	}

	line->raw = raw;

	if (name != NULL) {
		line->name = strdup(name);

		if (line->name == NULL) {
			errno = ENOMEM;

			goto error;
		}
	} else {
		line->name = NULL;
	}

	if (value != NULL) {
		line->value = strdup(value);

		if (line->value == NULL) {
			errno = ENOMEM;

			goto error;
		}
	} else {
		line->value = NULL;
	}

	return 0;

error:
	saved_errno = errno;

	array_remove(&conf_file->lines, conf_file->lines.count - 1,
	             conf_file_line_destroy);

	errno = saved_errno;

	return -1;
}

// sets errno on error
int conf_file_create(ConfFile *conf_file, uint32_t flags) {
	conf_file->flags = flags;

	return array_create(&conf_file->lines, 32, sizeof(ConfFileLine), true);
}

void conf_file_destroy(ConfFile *conf_file) {
	array_destroy(&conf_file->lines, conf_file_line_destroy);
}

// sets errno on error
int conf_file_read(ConfFile *conf_file, const char *filename,
                   ConfFileReadWarningFunction warning, void *opaque) {
	bool success = false;
	int allocated = 256;
	char *buffer;
	FILE *fp = NULL;
	size_t rc;
	char c;
	int length = 0;
	bool skip = false;
	int number = 1;
	char *tmp;
	int saved_errno;

	buffer = malloc(allocated);

	if (buffer == NULL) {
		errno = ENOMEM;

		goto cleanup;
	}

	fp = fopen(filename, "rb");

	if (fp == NULL) {
		errno = ENOENT;

		goto cleanup;
	}

	while (!feof(fp)) {
		rc = fread(&c, 1, 1, fp);

		if (rc == 0 && ferror(fp)) {
			errno = EIO;

			goto cleanup;
		}

		if (feof(fp) || c == '\n') {
			// end-of-file or end-of-line found. only use \n as end-of-line
			// marker. don't care about \r-only systems
			if (!skip) {
				// remove trailing \r if line ends with \r\n sequence
				if (length > 0 && buffer[length - 1] == '\r' && c == '\n') {
					buffer[--length] = '\0';
				}

				if (conf_file_parse_line(conf_file, number, buffer,
				                         warning, opaque) < 0) {
					goto cleanup;
				}
			}

			*buffer = '\0';
			length = 0;
			skip = false;
			++number;
		} else if (!skip) {
			if (length + 2 > allocated) {
				// not enough room for char and NULL-terminator
				if (allocated < 32768) {
					tmp = realloc(buffer, allocated * 2);

					if (tmp == NULL) {
						errno = ENOMEM;

						goto cleanup;
					}

					buffer = tmp;
					allocated *= 2;
				} else {
					// line is too long, skip it
					skip = true;

					if (warning != NULL) {
						// limit printed line length in log messages
						buffer[32] = '\0';

						warning(CONF_FILE_READ_WARNING_LINE_TOO_LONG,
						        number, buffer, opaque);
					}
				}
			}

			if (!skip) {
				buffer[length++] = c;
				buffer[length] = '\0';
			}
		}
	}

	success = true;

cleanup:
	saved_errno = errno;

	if (fp != NULL) {
		fclose(fp);
	}

	free(buffer);

	errno = saved_errno;

	return success ? 0 : -1;
}

const char *conf_file_get_option_value(ConfFile *conf_file, const char *name) {
	int i;
	ConfFileLine *line;

	// iterate backwards to make later instances of the same option
	// override earlier instances
	for (i = conf_file->lines.count - 1; i >= 0; --i) {
		line = array_get(&conf_file->lines, i);

		if (line->name != NULL && strcasecmp(line->name, name) == 0) {
			return line->value;
		}
	}

	return NULL;
}
