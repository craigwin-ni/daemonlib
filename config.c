/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 *
 * config.c: Config file subsystem
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "conf_file.h"
#include "utils.h"

static bool _check_only = false;
static bool _has_error = false;
static bool _has_warning = false;
static bool _using_default_values = true;
static ConfigOption _invalid = CONFIG_OPTION_STRING_INITIALIZER("<invalid>", NULL, 0, -1, "<invalid>");

extern ConfigOption config_options[];

#define config_error(...) config_message(&_has_error, __VA_ARGS__)
#define config_warn(...) config_message(&_has_warning, __VA_ARGS__)

static void config_message(bool *has_message, const char *format, ...) ATTRIBUTE_FMT_PRINTF(2, 3);

static void config_message(bool *has_message, const char *format, ...) {
	va_list arguments;

	*has_message = true;

	if (!_check_only) {
		return;
	}

	va_start(arguments, format);

	vfprintf(stderr, format, arguments);
	fprintf(stderr, "\n");
	fflush(stderr);

	va_end(arguments);
}

static void config_reset(void) {
	int i = 0;

	_using_default_values = true;

	for (i = 0; config_options[i].name != NULL; ++i) {
		if (config_options[i].type == CONFIG_OPTION_TYPE_STRING) {
			if (config_options[i].value.string != config_options[i].default_value.string) {
				free(config_options[i].value.string);
			}
		}

		memcpy(&config_options[i].value, &config_options[i].default_value,
		       sizeof(config_options[i].value));
	}
}

static int config_parse_int(const char *string, int *value) {
	char *end = NULL;
	long tmp;

	if (*string == '\0') {
		return -1;
	}

	tmp = strtol(string, &end, 10);

	if (end == NULL || *end != '\0') {
		return -1;
	}

	if (sizeof(long) > sizeof(int) && (tmp < INT32_MIN || tmp > INT32_MAX)) {
		return -1;
	}

	*value = tmp;

	return 0;
}

static int config_parse_log_level(const char *string, LogLevel *value) {
	LogLevel tmp;

	if (strcasecmp(string, "error") == 0) {
		tmp = LOG_LEVEL_ERROR;
	} else if (strcasecmp(string, "warn") == 0) {
		tmp = LOG_LEVEL_WARN;
	} else if (strcasecmp(string, "info") == 0) {
		tmp = LOG_LEVEL_INFO;
	} else if (strcasecmp(string, "debug") == 0) {
		tmp = LOG_LEVEL_DEBUG;
	} else {
		return -1;
	}

	*value = tmp;

	return 0;
}

static const char *config_format_log_level(LogLevel level) {
	switch (level) {
	case LOG_LEVEL_NONE:  return "none";
	case LOG_LEVEL_ERROR: return "error";
	case LOG_LEVEL_WARN:  return "warn";
	case LOG_LEVEL_INFO:  return "info";
	case LOG_LEVEL_DEBUG: return "debug";
	default:              return "<unknown>";
	}
}

static void config_report_read_warning(ConfFileReadWarning warning, int number,
                                       const char *buffer, void *opaque) {
	(void)opaque;

	switch (warning) {
	case CONF_FILE_READ_WARNING_LINE_TOO_LONG:
		config_warn("Line %d is too long: %s...", number, buffer);

		break;

	case CONF_FILE_READ_WARNING_NAME_MISSING:
		config_warn("Line %d has no option name: %s", number, buffer);

		break;

	case CONF_FILE_READ_WARNING_EQUAL_SIGN_MISSING:
		config_warn("Line %d has no '=' sign: %s", number, buffer);

		break;

	default:
		config_warn("Unknown warning %d in line %d", warning, number);

		break;
	}
}

int config_check(const char *filename) {
	bool success = false;
	int i;

	_check_only = true;

	config_init(filename);

	if (_has_error) {
		fprintf(stderr, "Error(s) occurred while reading config file '%s'\n", filename);

		goto cleanup;
	}

	if (_has_warning) {
		fprintf(stderr, "Warning(s) in config file '%s'\n", filename);

		goto cleanup;
	}

	if (_using_default_values) {
		printf("Config file '%s' not found, using default values\n", filename);
	} else {
		printf("No warnings or errors in config file '%s'\n", filename);
	}

	printf("\n");
	printf("Using the following config values:\n");

	for (i = 0; config_options[i].name != NULL; ++i) {
		printf("  %s = ", config_options[i].name);

		switch(config_options[i].type) {
		case CONFIG_OPTION_TYPE_STRING:
			if (config_options[i].value.string != NULL) {
				printf("%s", config_options[i].value.string);
			}

			break;

		case CONFIG_OPTION_TYPE_INTEGER:
			printf("%d", config_options[i].value.integer);

			break;

		case CONFIG_OPTION_TYPE_BOOLEAN:
			printf("%s", config_options[i].value.boolean ? "on" : "off");

			break;

		case CONFIG_OPTION_TYPE_LOG_LEVEL:
			printf("%s", config_format_log_level(config_options[i].value.log_level));

			break;

		default:
			printf("<unknown-type>");

			break;
		}

		printf("\n");
	}

	success = true;

cleanup:
	config_exit();

	return success ? 0 : -1;
}

void config_init(const char *filename) {
	ConfFile conf_file;
	int i;
	const char *name;
	const char *value;
	int length;
	int integer;

	config_reset();

	// read config file
	if (conf_file_create(&conf_file, CONF_FILE_FLAG_TRIM_VALUE_ON_READ) < 0) {
		config_error("Internal error occurred");

		return;
	}

	if (conf_file_read(&conf_file, filename, config_report_read_warning, NULL) < 0) {
		if (errno == ENOENT) {
			// ignore
		} else if (errno == ENOMEM) {
			config_error("Could not allocate memory");
		} else {
			config_error("Error %s (%d) occurred", get_errno_name(errno), errno);
		}

		goto cleanup;
	}

	_using_default_values = false;

	for (i = 0; config_options[i].name != NULL; ++i) {
		name = config_options[i].name;
		value = conf_file_get_option_value(&conf_file, name);

		if (value == NULL && config_options[i].legacy_name != NULL) {
			name = config_options[i].legacy_name;
			value = conf_file_get_option_value(&conf_file, name);
		}

		if (value == NULL) {
			continue;
		}

		switch (config_options[i].type) {
		case CONFIG_OPTION_TYPE_STRING:
			if (config_options[i].value.string != config_options[i].default_value.string) {
				free(config_options[i].value.string);
				config_options[i].value.string = NULL;
			}

			length = strlen(value);

			if (length < config_options[i].string_min_length) {
				config_warn("Value '%s' for %s option is too short (minimum: %d chars)",
				            value, name, config_options[i].string_min_length);

				goto cleanup;
			} else if (config_options[i].string_max_length >= 0 &&
			           length > config_options[i].string_max_length) {
				config_warn("Value '%s' for %s option is too long (maximum: %d chars)",
				            value, name, config_options[i].string_max_length);

				goto cleanup;
			} else if (length > 0) {
				config_options[i].value.string = strdup(value);

				if (config_options[i].value.string == NULL) {
					config_error("Could not duplicate %s value '%s'", name, value);

					goto cleanup;
				}
			}

			break;

		case CONFIG_OPTION_TYPE_INTEGER:
			if (config_parse_int(value, &integer) < 0) {
				config_warn("Value '%s' for %s option is not an integer", value, name);

				goto cleanup;
			}

			if (integer < config_options[i].integer_min || integer > config_options[i].integer_max) {
				config_warn("Value %d for %s option is out-of-range (minimum: %d, maximum: %d)",
				            integer, name, config_options[i].integer_min, config_options[i].integer_max);

				goto cleanup;
			}

			config_options[i].value.integer = integer;

			break;

		case CONFIG_OPTION_TYPE_BOOLEAN:
			if (strcasecmp(value, "on") == 0) {
				config_options[i].value.boolean = true;
			} else if (strcasecmp(value, "off") == 0) {
				config_options[i].value.boolean = false;
			} else {
				config_warn("Value '%s' for %s option is invalid", value, name);

				goto cleanup;
			}

			break;

		case CONFIG_OPTION_TYPE_LOG_LEVEL:
			if (config_parse_log_level(value, &config_options[i].value.log_level) < 0) {
				config_warn("Value '%s' for %s option is invalid", value, name);

				goto cleanup;
			}

			break;
		}
	}

cleanup:
	conf_file_destroy(&conf_file);
}

void config_exit(void) {
	int i = 0;

	for (i = 0; config_options[i].name != NULL; ++i) {
		if (config_options[i].type == CONFIG_OPTION_TYPE_STRING) {
			if (config_options[i].value.string != config_options[i].default_value.string) {
				free(config_options[i].value.string);
			}
		}
	}
}

bool config_has_error(void) {
	return _has_error;
}

bool config_has_warning(void) {
	return _has_warning;
}

ConfigOptionValue *config_get_option_value(const char *name) {
	int i = 0;

	for (i = 0; config_options[i].name != NULL; ++i) {
		if (strcmp(config_options[i].name, name) == 0) {
			return &config_options[i].value;
		}
	}

	return &_invalid.value;
}
