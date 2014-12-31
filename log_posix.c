/*
 * daemonlib
 * Copyright (C) 2012, 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * log_posix.c: POSIX specific log handling
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "log.h"

bool _log_debug_override_platform = false;

static FILE *_file = NULL;

void log_set_file_platform(FILE *file);

void log_init_platform(FILE *file) {
	log_set_file_platform(file);
}

void log_exit_platform(void) {
}

void log_set_file_platform(FILE *file) {
	char *term;

	_file = NULL;

	if (file == NULL) {
		return;
	}

	if (!isatty(fileno(file))) {
		return;
	}

	term = getenv("TERM");

	if (term == NULL || strcmp(term, "dumb") == 0) {
		return;
	}

	_file = file;
}

void log_apply_color_platform(LogLevel level, bool begin) {
	const char *color = "";

	if (_file == NULL) {
		return;
	}

	if (begin) {
		switch (level) {
		case LOG_LEVEL_ERROR: color = "\033[1;31m"; break; // bold + red
		// FIXME: yellow would be better for warning, but yellow has poor
		//        contrast on white background. there seems to be no reasonable
		//        way to detect the current terminal background color to
		//        dynamically adapt the colors for good contrast. as workaround
		//        switch from yellow (3) to blue (4)
		case LOG_LEVEL_WARN:  color = "\033[1;34m"; break; // bold + blue
		case LOG_LEVEL_INFO:  color = "\033[1m";    break; // bold
		default:
		case LOG_LEVEL_DEBUG:                       return;
		}
	} else {
		switch (level) {
		case LOG_LEVEL_ERROR:
		case LOG_LEVEL_WARN:
		case LOG_LEVEL_INFO:  color = "\033[m";     break; // default
		default:
		case LOG_LEVEL_DEBUG:                       return;
		}
	}

	fprintf(_file, "%s", color);
}

bool log_is_message_included_platform(LogLevel level) {
	(void)level;

	return false;
}

// NOTE: assumes that _mutex (in log.c) is locked
void log_secondary_output_platform(struct timeval *timestamp, LogLevel level,
                                   LogSource *source, int line,
                                   const char *format, va_list arguments) {
	(void)timestamp;
	(void)level;
	(void)source;
	(void)line;
	(void)format;
	(void)arguments;
}
