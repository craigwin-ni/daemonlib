/*
 * daemonlib
 * Copyright (C) 2012, 2014, 2016-2017, 2019-2021 Matthias Bolte <matthias@tinkerforge.com>
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

#include "log.h"

#include "config.h"
#include "fifo.h"
#include "threads.h"
#include "utils.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

#define MAX_DEBUG_FILTERS 64
#define MAX_SOURCE_NAME_SIZE 64 // bytes
#define MAX_OUTPUT_SIZE (5 * 1024 * 1024) // bytes
#define MAX_ROTATE_COUNTDOWN 50
#define MAX_FIFO_BUFFER_SIZE (256 * 1024) // bytes

typedef struct {
	bool included;
	char source_name[MAX_SOURCE_NAME_SIZE + 1]; // +1 for NUL-terminator
	int line;
	uint32_t groups;
} LogDebugFilter;

typedef struct {
	struct timeval timestamp;
	LogLevel level;
	LogSource *source;
	LogDebugGroup debug_group;
	uint32_t inclusion;
	const char *function;
	int line;
} LogEntry;

static Mutex _common_mutex;
static LogLevel _level;
static Mutex _output_mutex; // protects writing to _output, _output_size, _rotate and _rotate_countdown
static IO *_output;
static int64_t _output_size; // tracks size if output is rotatable
static LogRotateFunction _rotate;
static int _rotate_countdown;
static uint8_t _fifo_buffer[MAX_FIFO_BUFFER_SIZE];
static FIFO _fifo;
static Thread _forward_thread;
static bool _debug_override;
static int _debug_filter_version;
static LogDebugFilter _debug_filters[MAX_DEBUG_FILTERS];
static int _debug_filter_count;

IO log_stderr_output;

extern void log_init_platform(IO *output);
extern void log_exit_platform(void);
extern void log_set_output_platform(IO *output);
extern void log_apply_color_platform(LogLevel level, bool begin);
extern uint32_t log_check_inclusion_platform(LogLevel level, LogSource *source,
                                             LogDebugGroup debug_group, int line);
extern void log_output_platform(struct timeval *timestamp, LogLevel level,
                                LogSource *source, LogDebugGroup debug_group,
                                const char *function, int line, const char *message);

static int stderr_write(IO *io, const void *buffer, int length) {
	int rc;

	(void)io;

	rc = robust_fwrite(stderr, buffer, length);

	fflush(stderr);

	return rc;
}

static int stderr_create(IO *io) {
	if (io_create(io, "stderr", NULL, NULL, stderr_write, NULL) < 0) {
		return -1;
	}

	io->write_handle = fileno(stderr);

	return 0;
}

static void log_timestamp(struct timeval *timestamp) {
	if (gettimeofday(timestamp, NULL) < 0) {
		timestamp->tv_sec = time(NULL);
		timestamp->tv_usec = 0;
	}
}

static bool log_set_debug_filter(const char *filter) {
	const char *p = filter;
	int i = 0;
	int k;
	int line;

	++_debug_filter_version;
	_debug_filter_count = 0;

	while (*p != '\0') {
		if (i >= MAX_DEBUG_FILTERS) {
			log_warn("Too many source names in debug filter '%s'", filter);

			return false;
		}

		// parse +/-
		if (*p == '+') {
			_debug_filters[i].included = true;
		} else if (*p == '-') {
			_debug_filters[i].included = false;
		} else {
			log_warn("Unexpected char '%c' in debug filter '%s' at index %d",
			         *p, filter, (int)(p - filter));

			return false;
		}

		++p; // skip +/-

		// parse source name and line
		k = 0;
		line = -1;

		while (*p != '\0' && *p != ',') {
			if (*p == ':') {
				if (k == 0) {
					log_warn("Empty source name in debug filter '%s' at index %d",
					         filter, (int)(p - filter));

					return false;
				}

				++p; // skip colon
				line = 0;

				while (*p != '\0' && *p != ',') {
					if (*p >= '0' && *p <= '9') {
						if (line > 100000) {
							log_warn("Invalid line number in debug filter '%s' at index %d",
							         filter, (int)(p - filter));

							return false;
						}

						line = (line * 10) + (*p - '0');
					} else {
						log_warn("Unexpected char '%c' in debug filter '%s' at index %d",
						         *p, filter, (int)(p - filter));

						return false;
					}

					++p; // skip digit
				}

				if (line == 0) {
					log_warn("Invalid line number in debug filter '%s' at index %d",
					         filter, (int)(p - filter));

					return false;
				}

				break;
			} else {
				if (k >= MAX_SOURCE_NAME_SIZE) {
					log_warn("Source name '%s' is too long in debug filter '%s' at index %d",
					         p, filter, (int)(p - filter));

					return false;
				}

				_debug_filters[i].source_name[k++] = *p++;
			}
		}

		if (k == 0) {
			log_warn("Empty source name in debug filter '%s' at index %d",
			         filter, (int)(p - filter));

			return false;
		}

		_debug_filters[i].source_name[k] = '\0';
		_debug_filters[i].line = line;

		if (strcasecmp(_debug_filters[i].source_name, "common") == 0) {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_COMMON;
		} else if (strcasecmp(_debug_filters[i].source_name, "event") == 0) {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_EVENT;
		} else if (strcasecmp(_debug_filters[i].source_name, "packet") == 0) {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_PACKET;
		} else if (strcasecmp(_debug_filters[i].source_name, "object") == 0) {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_OBJECT;
		} else if (strcasecmp(_debug_filters[i].source_name, "libusb") == 0) {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_LIBUSB;
		} else if (strcasecmp(_debug_filters[i].source_name, "all") == 0) {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_ALL;
		} else {
			_debug_filters[i].groups = LOG_DEBUG_GROUP_NONE;
		}

		if (_debug_filters[i].groups != LOG_DEBUG_GROUP_NONE) {
			if (line >= 0) {
				log_warn("Ignoring line number for source name '%s' in debug filter '%s' at index %d",
				         _debug_filters[i].source_name, filter, (int)(p - filter));
			}

			_debug_filters[i].source_name[0] = '\0';
			_debug_filters[i].line = -1;
		}

		if (*p == ',') {
			++p; // skip comma

			if (*p == '\0') {
				log_warn("Debug filter '%s' ends with a trailing comma", filter);

				return false;
			}
		}

		++i;
	}

	_debug_filter_count = i;

	return true;
}

// NOTE: assumes that _output_mutex is locked
static void log_set_output_unlocked(IO *output, LogRotateFunction rotate) {
	IOStatus status;

	_output = output;
	_output_size = -1;
	_rotate = rotate;
	_rotate_countdown = MAX_ROTATE_COUNTDOWN;

	if (_output != NULL && _rotate != NULL) {
		if (io_status(_output, &status) >= 0) {
			_output_size = status.size;
		}
	}

	log_set_output_platform(_output);
}

// NOTE: assumes that _output_mutex is locked
static void log_output(LogEntry *entry, const char *message) {
	char buffer[1024];
	int length;

	if ((entry->inclusion & LOG_INCLUSION_PRIMARY) != 0 && _output != NULL) {
		length = log_format(buffer, sizeof(buffer), &entry->timestamp, entry->level,
		                    entry->source, entry->debug_group, entry->function, entry->line,
		                    message);

		log_apply_color_platform(entry->level, true);

		length = io_write(_output, buffer, length);

		log_apply_color_platform(entry->level, false);

		if (_output_size >= 0 && length >= 0) {
			_output_size += length;
		}
	}

	if ((entry->inclusion & LOG_INCLUSION_SECONDARY) != 0) {
		log_output_platform(&entry->timestamp, entry->level, entry->source,
		                    entry->debug_group, entry->function, entry->line,
		                    message);
	}
}

static void log_forward(void *opaque) {
	union {
		char buffer[8192];
		LogEntry entry;
	} u;
	int length;
	int buffer_used = 0;
	int message_length;
	LogLevel rotate_level;
	int rotate_line;
	LogDebugGroup rotate_debug_group;
	uint32_t rotate_inclusion;
	char rotate_message[1024];
	LogEntry rotate_entry;

	(void)opaque;

	memset(u.buffer, 0, sizeof(u.buffer));

	while (true) {
		length = fifo_read(&_fifo, u.buffer + buffer_used, sizeof(u.buffer) - buffer_used, 0);

		if (length < 0) {
			break; // FIXME
		}

		if (length == 0) {
			break;
		}

		buffer_used += length;

		while (buffer_used > 0) {
			if (buffer_used < (int)sizeof(u.entry)) {
				break; // wait for complete LogEntry
			}

			message_length = strnlen(u.buffer + sizeof(u.entry), buffer_used - sizeof(u.entry));

			if (message_length >= buffer_used - (int)sizeof(u.entry)) {
				break; // wait for complete NUL-terminated message
			}

			mutex_lock(&_output_mutex);

			log_output(&u.entry, u.buffer + sizeof(u.entry));

			if (_rotate_countdown > 0) {
				--_rotate_countdown;
			}

			rotate_level = LOG_LEVEL_NONE;

			if (_rotate != NULL && _rotate_countdown <= 0 && _output_size >= MAX_OUTPUT_SIZE) {
				string_copy(rotate_message, sizeof(rotate_message), "<unknown>", -1);

				if (_rotate(_output, &rotate_level, rotate_message, sizeof(rotate_message)) < 0) {
					log_set_output_unlocked(NULL, NULL);
				} else {
					log_set_output_unlocked(_output, _rotate);
				}
			}

			mutex_unlock(&_output_mutex);

			if (rotate_level != LOG_LEVEL_NONE) {
				if (rotate_level == LOG_LEVEL_DEBUG) {
					rotate_debug_group = LOG_DEBUG_GROUP_COMMON;
				} else {
					rotate_debug_group = LOG_DEBUG_GROUP_NONE;
				}

				rotate_line = __LINE__;
				rotate_inclusion = log_check_inclusion(rotate_level, &_log_source,
				                                       rotate_debug_group, rotate_line);

				if (rotate_inclusion != LOG_INCLUSION_NONE) {
					log_timestamp(&rotate_entry.timestamp);

					rotate_entry.level = rotate_level;
					rotate_entry.source = &_log_source;
					rotate_entry.debug_group = rotate_debug_group;
					rotate_entry.inclusion = rotate_inclusion;
					rotate_entry.function = __FUNCTION__;
					rotate_entry.line = rotate_line;

					log_output(&rotate_entry, rotate_message);
				}
			}

			memmove(u.buffer, u.buffer + sizeof(u.entry) + message_length + 1,
			        buffer_used - sizeof(u.entry) - message_length - 1);

			buffer_used -= sizeof(u.entry) + message_length + 1;
		}
	}
}

void log_init(void) {
	const char *filter;

	mutex_create(&_common_mutex);
	mutex_create(&_output_mutex);

	_level = config_get_option_value("log.level")->symbol;

	stderr_create(&log_stderr_output);

	_output = &log_stderr_output;
	_output_size = -1;
	_rotate = NULL;
	_rotate_countdown = 0;

	fifo_create(&_fifo, _fifo_buffer, sizeof(_fifo_buffer));

	_debug_override = false;
	_debug_filter_version = 0;
	_debug_filter_count = 0;

	thread_create(&_forward_thread, log_forward, NULL);

	log_init_platform(_output);

	filter = config_get_option_value("log.debug_filter")->string;

	if (filter != NULL) {
		log_set_debug_filter(filter);
	}
}

void log_exit(void) {
	log_exit_platform();

	fifo_shutdown(&_fifo);

	thread_join(&_forward_thread);
	thread_destroy(&_forward_thread);

	fifo_destroy(&_fifo);

	mutex_destroy(&_output_mutex);
	mutex_destroy(&_common_mutex);
}

void log_enable_debug_override(const char *filter) {
	_debug_override = log_set_debug_filter(filter);
}

LogLevel log_get_effective_level(void) {
	return _debug_override ? LOG_LEVEL_DEBUG : _level;
}

// if a ROTATE function is given, then the given OUTPUT has to support io_status
void log_set_output(IO *output, LogRotateFunction rotate) {
	mutex_lock(&_output_mutex);

	log_set_output_unlocked(output, rotate);

	mutex_unlock(&_output_mutex);
}

void log_get_output(IO **output, LogRotateFunction *rotate) {
	mutex_lock(&_output_mutex);

	if (output != NULL) {
		*output = _output;
	}

	if (rotate != NULL) {
		*rotate = _rotate;
	}

	mutex_unlock(&_output_mutex);
}

uint32_t log_check_inclusion(LogLevel level, LogSource *source,
                             LogDebugGroup debug_group, int line) {
	uint32_t result;
	const char *p;
	int i;
	int k;
	LogSourceLine *source_line;

	// if the source name is not set yet use the last part of its __FILE__
	if (source->name == NULL) {
		mutex_lock(&_common_mutex);

		// after gaining the mutex check that nobody else has set the name in the meantime
		if (source->name == NULL) {
			source->name = source->file;
			p = strrchr(source->name, '/');

			if (p != NULL) {
				source->name = p + 1;
			}

			p = strrchr(source->name, '\\');

			if (p != NULL) {
				source->name = p + 1;
			}
		}

		mutex_unlock(&_common_mutex);
	}

	result = log_check_inclusion_platform(level, source, debug_group, line);

	if (!_debug_override && level > _level) {
		// primary output excluded by level
		return result;
	}

	if (level != LOG_LEVEL_DEBUG) {
		return result | LOG_INCLUSION_PRIMARY;
	}

	// check if source's debug-groups are outdated, if yes try to update them
	if (source->debug_filter_version < _debug_filter_version) {
		mutex_lock(&_common_mutex);

		// after gaining the mutex check that the source's debug-groups are
		// still outdated and nobody else has updated them in the meantime
		if (source->debug_filter_version < _debug_filter_version) {
			source->debug_filter_version = _debug_filter_version;
			source->included_debug_groups = LOG_DEBUG_GROUP_ALL;
			source->lines_used = 0;

			for (i = 0; i < _debug_filter_count; ++i) {
				if (strcasecmp(_debug_filters[i].source_name, source->name) == 0) {
					if (_debug_filters[i].line < 0) {
						if (_debug_filters[i].included) {
							source->included_debug_groups = LOG_DEBUG_GROUP_ALL;
						} else {
							source->included_debug_groups = LOG_DEBUG_GROUP_NONE;
						}
					} else {
						source_line = NULL;

						for (k = 0; k < source->lines_used; ++k) {
							if (source->lines[k].number == _debug_filters[i].line) {
								source_line = &source->lines[k];
								break;
							}
						}

						if (source_line == NULL) {
							if (source->lines_used >= LOG_MAX_SOURCE_LINES) {
								// to many lines for this source
								continue; // FIXME: how to report this to the user?
							}

							source_line = &source->lines[source->lines_used++];
							source_line->number = _debug_filters[i].line;
						}

						if (_debug_filters[i].included) {
							source_line->included_debug_groups = LOG_DEBUG_GROUP_ALL;
						} else {
							source_line->included_debug_groups = LOG_DEBUG_GROUP_NONE;
						}
					}
				} else {
					if (_debug_filters[i].included) {
						source->included_debug_groups |= _debug_filters[i].groups;
					} else {
						source->included_debug_groups &= ~_debug_filters[i].groups;
					}

					for (k = 0; k < source->lines_used; ++k) {
						source_line = &source->lines[k];

						if (_debug_filters[i].included) {
							source_line->included_debug_groups |= _debug_filters[i].groups;
						} else {
							source_line->included_debug_groups &= ~_debug_filters[i].groups;
						}
					}
				}
			}
		}

		mutex_unlock(&_common_mutex);
	}

	// now the debug-groups are guaranteed to be up to date
	if (line > 0) {
		for (k = 0; k < source->lines_used; ++k) {
			if (source->lines[k].number == line) {
				if ((source->lines[k].included_debug_groups & debug_group) != 0) {
					return result | LOG_INCLUSION_PRIMARY;
				}

				// primary output excluded by debug-groups
				return result;
			}
		}
	}

	if ((source->included_debug_groups & debug_group) != 0) {
		return result | LOG_INCLUSION_PRIMARY;
	}

	// primary output excluded by debug-groups
	return result;
}

void log_message(LogLevel level, LogSource *source, LogDebugGroup debug_group,
                 uint32_t inclusion, const char *function, int line,
                 const char *format, ...) {
	LogEntry entry;
	va_list arguments;
	char message[1024] = "<unknown>";
	int message_length;

	if (level == LOG_LEVEL_NONE || inclusion == LOG_INCLUSION_NONE) {
		return; // should never be reachable
	}

	// record timestamp before locking the mutex. this results in more
	// accurate timing of log message if the common mutex is contended
	log_timestamp(&entry.timestamp);

	entry.level = level;
	entry.source = source;
	entry.debug_group = debug_group;
	entry.inclusion = inclusion;
	entry.function = function;
	entry.line = line;

	va_start(arguments, format);

	message_length = vsnprintf(message, sizeof(message), format, arguments);

	va_end(arguments);

	message_length = MIN(message_length, (int)sizeof(message) - 1);

	mutex_lock(&_common_mutex);

	fifo_write(&_fifo, &entry, sizeof(entry), 0);
	fifo_write(&_fifo, message, message_length + 1, 0);

	mutex_unlock(&_common_mutex);
}

int log_format(char *buffer, int length, struct timeval *timestamp,
               LogLevel level, LogSource *source, LogDebugGroup debug_group,
               const char *function, int line, const char *message) {
	time_t unix_seconds;
	struct tm localized_timestamp;
	char formatted_timestamp[64] = "<unknown>";
	char formatted_timestamp_usec[16] = "<unknown>";
	char *level_str;
	char *debug_group_name = "";
	char line_str[16] = "<unknown>";
#ifdef _WIN32
	int newline_length = 2;
#else
	int newline_length = 1;
#endif
	int formatted_length;

	if (length < 1) {
		return 0;
	}

	// format time
	if (timestamp == NULL) {
		formatted_timestamp[0] = '\0';
		formatted_timestamp_usec[0] = '\0';
	} else {
		// copy value to time_t variable because timeval.tv_sec and time_t
		// can have different sizes between different compilers and compiler
		// version and platforms. for example with WDK 7 both are 4 byte in
		// size, but with MSVC 2010 time_t is 8 byte in size but timeval.tv_sec
		// is still 4 byte in size.
		unix_seconds = timestamp->tv_sec;

		if (localtime_r(&unix_seconds, &localized_timestamp) != NULL) {
			strftime(formatted_timestamp, sizeof(formatted_timestamp),
			         "%Y-%m-%d %H:%M:%S", &localized_timestamp);
		}

		snprintf(formatted_timestamp_usec, sizeof(formatted_timestamp_usec),
		         ".%06d ", (int)timestamp->tv_usec);
	}

	// format level
	switch (level) {
	case LOG_LEVEL_NONE:  level_str = ""; break;
	case LOG_LEVEL_ERROR: level_str = "<E> "; break;
	case LOG_LEVEL_WARN:  level_str = "<W> "; break;
	case LOG_LEVEL_INFO:  level_str = "<I> "; break;
	case LOG_LEVEL_DEBUG: level_str = "<D> "; break;
	default:              level_str = "<U> "; break;
	}

	// format debug group
	switch (debug_group) {
	case LOG_DEBUG_GROUP_EVENT:  debug_group_name = "event|";  break;
	case LOG_DEBUG_GROUP_PACKET: debug_group_name = "packet|"; break;
	case LOG_DEBUG_GROUP_OBJECT: debug_group_name = "object|"; break;
	case LOG_DEBUG_GROUP_LIBUSB:                               break;
	default:                                                   break;
	}

	// format line
	if (line >= 0) {
		snprintf(line_str, sizeof(line_str), "%d", line);
	}

	// format output
	formatted_length = snprintf(buffer, length - newline_length, "%s%s%s<%s%s%s%s> %s",
	                            formatted_timestamp, formatted_timestamp_usec, level_str,
	                            debug_group_name, source->name, line >= 0 || function != NULL ? ":" : "",
	                            line >= 0 ? line_str : (function != NULL ? function : ""), message);

	if (formatted_length < 0) {
		buffer[0] = '\0';

		return 0;
	}

	formatted_length = MIN(formatted_length, length - newline_length - 1);

	// append newline
#ifdef _WIN32
	buffer[formatted_length++] = '\r';
	buffer[formatted_length++] = '\n';
#else
	buffer[formatted_length++] = '\n';
#endif

	buffer[formatted_length] = '\0';

	return formatted_length;
}
