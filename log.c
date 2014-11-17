/*
 * daemonlib
 * Copyright (C) 2012, 2014 Matthias Bolte <matthias@tinkerforge.com>
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 *
 * log.c: Logging specific functions
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
#include <string.h>
#ifndef _MSC_VER
	#include <sys/time.h>
#endif

#include "log.h"

#include "config.h"
#include "threads.h"

static Mutex _mutex; // protects writing to _file
static bool _debug_override = false;
static LogLevel _level = LOG_LEVEL_INFO;
static FILE *_file = NULL;

extern bool _log_debug_override_platform;

extern void log_init_platform(FILE *file);
extern void log_exit_platform(void);
extern void log_set_file_platform(FILE *file);
extern void log_apply_color_platform(LogLevel level, bool begin);
extern void log_secondary_output_platform(struct timeval *timestamp, LogLevel level,
                                          const char *filename, int line,
                                          const char *function, const char *format,
                                          va_list arguments);

// NOTE: assumes that _mutex is locked
static void log_primary_output(struct timeval *timestamp, LogLevel level,
                               const char *filename, int line, const char *function,
                               const char *format, va_list arguments) {
	time_t unix_seconds;
	struct tm localized_timestamp;
	char formatted_timestamp[64] = "<unknown>";
	char level_char;

	(void)function;

	// check file
	if (_file == NULL) {
		return;
	}

	// copy value to time_t variable because timeval.tv_sec and time_t
	// can have different sizes between different compilers and compiler
	// version and platforms. for example with WDK 7 both are 4 byte in
	// size, but with MSVC 2010 time_t is 8 byte in size but timeval.tv_sec
	// is still 4 byte in size.
	unix_seconds = timestamp->tv_sec;

	// format time
	if (localtime_r(&unix_seconds, &localized_timestamp) != NULL) {
		strftime(formatted_timestamp, sizeof(formatted_timestamp),
		         "%Y-%m-%d %H:%M:%S", &localized_timestamp);
	}

	// format level
	switch (level) {
	case LOG_LEVEL_ERROR: level_char = 'E'; break;
	case LOG_LEVEL_WARN:  level_char = 'W'; break;
	case LOG_LEVEL_INFO:  level_char = 'I'; break;
	case LOG_LEVEL_DEBUG: level_char = 'D'; break;
	default:              level_char = 'U'; break;
	}

	// begin color
	log_apply_color_platform(level, true);

	// print prefix
	fprintf(_file, "%s.%06d <%c> <%s:%d> ",
	        formatted_timestamp, (int)timestamp->tv_usec, level_char, filename, line);

	// print message
	vfprintf(_file, format, arguments);
	fprintf(_file, "\n");

	// end color
	log_apply_color_platform(level, false);

	fflush(_file);
}

void log_init(void) {
	mutex_create(&_mutex);

	_level = config_get_option_value("log.level")->symbol;
	_file = stderr;

	log_init_platform(_file);
}

void log_exit(void) {
	log_exit_platform();

	mutex_destroy(&_mutex);
}

void log_lock(void) {
	mutex_lock(&_mutex);
}

void log_unlock(void) {
	mutex_unlock(&_mutex);
}

void log_set_debug_override(bool override) {
	_debug_override = override;
}

LogLevel log_get_effective_level(void) {
	if (_debug_override || _log_debug_override_platform) {
		return LOG_LEVEL_DEBUG;
	} else {
		return _level;
	}
}

void log_set_file(FILE *file) {
	log_lock();

	_file = file;

	log_set_file_platform(file);

	log_unlock();
}

FILE *log_get_file(void) {
	return _file;
}

void log_message(LogLevel level, const char *filename, int line,
                 const char *function, const char *format, ...) {
	struct timeval timestamp;
	const char *p;
	va_list arguments;

	if (level == LOG_LEVEL_DUMMY) {
		return; // should never be reachable
	}

	// record timestamp before locking the mutex. this results in more accurate
	// timing of log message if the mutex is contended
	if (gettimeofday(&timestamp, NULL) < 0) {
		timestamp.tv_sec = time(NULL);
		timestamp.tv_usec = 0;
	}

	// only keep last part of filename
	p = strrchr(filename, '/');

	if (p != NULL) {
		filename = p + 1;
	}

	p = strrchr(filename, '\\');

	if (p != NULL) {
		filename = p + 1;
	}

	// call log handlers
	va_start(arguments, format);
	log_lock();

	if (level <= _level || _debug_override) {
		log_primary_output(&timestamp, level, filename, line, function, format, arguments);
	}

	if (level <= _level || _debug_override || _log_debug_override_platform) {
		log_secondary_output_platform(&timestamp, level, filename, line, function, format, arguments);
	}

	log_unlock();
	va_end(arguments);
}
