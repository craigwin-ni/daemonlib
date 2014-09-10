/*
 * daemonlib
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * conf_file.h: Reads and writes .conf formatted files
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

#ifndef DAEMONLIB_CONF_FILE_H
#define DAEMONLIB_CONF_FILE_H

#include "array.h"

typedef enum { // bitmask
	CONF_FILE_FLAG_TRIM_VALUE_ON_READ = 0x0001
} ConfFileFlag;

typedef enum {
	CONF_FILE_READ_WARNING_LINE_TOO_LONG = 0,
	CONF_FILE_READ_WARNING_NAME_MISSING,
	CONF_FILE_READ_WARNING_EQUAL_SIGN_MISSING
} ConfFileReadWarning;

typedef void (*ConfFileReadWarningFunction)(ConfFileReadWarning warning,
                                            int number, const char *buffer,
                                            void *opaque);

typedef struct {
	char *raw;
	char *name; // case of the name is ignored
	char *value;
} ConfFileLine;

typedef struct {
	uint32_t flags;
	Array lines;
} ConfFile;

int conf_file_create(ConfFile *conf_file, uint32_t flags);
void conf_file_destroy(ConfFile *conf_file);

int conf_file_read(ConfFile *conf_file, const char *filename,
                   ConfFileReadWarningFunction warning, void *opaque);

const char *conf_file_get_option_value(ConfFile *conf_file, const char *name);

#endif // DAEMONLIB_CONF_FILE_H
