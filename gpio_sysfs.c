/*
 * daemonlib
 * Copyright (C) 2018 Olaf Lüke <olaf@tinkerforge.com>
 *
 * gpio_sysfs.c: GPIO functions for using linux sysfs
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

#include "gpio_sysfs.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"
#include "utils.h"

#define GPIO_SYSFS_DIR "/sys/class/gpio/"
#define GPIO_SYSFS_DIR_MAXLEN 256

#define GPIO_SYSFS_INTERRUPT_NUM 4
#define GPIO_SYSFS_DIRECTION_NUM 2
#define GPIO_SYSFS_VALUE_NUM 2

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

static const char * const gpio_sysfs_interrupt[GPIO_SYSFS_INTERRUPT_NUM] = {"none", "rising", "falling", "both"};
static const char * const gpio_sysfs_direction[GPIO_SYSFS_DIRECTION_NUM] = {"in", "out"};
static const char * const gpio_sysfs_value[GPIO_SYSFS_DIRECTION_NUM] = {"0", "1"};

int gpio_sysfs_export(GPIOSYSFS *gpio) {
	int fd;
	char buffer[GPIO_SYSFS_DIR_MAXLEN];
	int length;
	int rc;

	fd = open(GPIO_SYSFS_DIR "export", O_WRONLY);

	if (fd < 0) {
		log_error("Could not open '%s': %s (%d)", GPIO_SYSFS_DIR "export", get_errno_name(errno), errno);
		return -1;
	}

	length = snprintf(buffer, sizeof(buffer), "%d", gpio->num);
	rc = robust_write(fd, buffer, length);

	close(fd);

	if (rc < 0 && errno == EBUSY) {
		return 0; // gpio was already exported
	}

	if (rc < 0) {
		log_error("Could not write to '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	return 0;
}

int gpio_sysfs_unexport(GPIOSYSFS *gpio) {
	int fd;
	char buffer[GPIO_SYSFS_DIR_MAXLEN];
	int length;
	int rc;

	fd = open(GPIO_SYSFS_DIR "unexport", O_WRONLY);

	if (fd < 0) {
		log_error("Could not open '%s': %s (%d)", GPIO_SYSFS_DIR "unexport", get_errno_name(errno), errno);
		return -1;
	}

	length = snprintf(buffer, sizeof(buffer), "%d", gpio->num);
	rc = robust_write(fd, buffer, length);

	close(fd);

	if (rc < 0) {
		log_error("Could not write to '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	return 0;
}

int gpio_sysfs_set_direction(GPIOSYSFS *gpio, GPIOSYSFSDirection direction) {
	int fd;
	char buffer[GPIO_SYSFS_DIR_MAXLEN];
	int rc;

    if(direction >= GPIO_SYSFS_DIRECTION_NUM) {
		log_error("Unknown direction: %d", direction);
        return -1;
    }

	snprintf(buffer, sizeof(buffer), "%s%s/direction", GPIO_SYSFS_DIR, gpio->name);

	fd = open(buffer, O_WRONLY);

	if (fd < 0) {
		log_error("Could not open '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	rc = robust_write(fd, gpio_sysfs_direction[direction], strlen(gpio_sysfs_direction[direction]));

	close(fd);

	if (rc < 0) {
		log_error("Could not write to '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	return 0;
}

int gpio_sysfs_set_output(GPIOSYSFS *gpio, GPIOSYSFSValue value) {
	int fd;
	char buffer[GPIO_SYSFS_DIR_MAXLEN];
	int rc;

    if(value >= GPIO_SYSFS_VALUE_NUM) {
		log_error("Unknown value: %d", value);
        return -1;
    }

	snprintf(buffer, sizeof(buffer), "%s%s/value", GPIO_SYSFS_DIR, gpio->name);

	fd = open(buffer, O_WRONLY);

	if (fd < 0) {
		log_error("Could not open '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	rc = robust_write(fd, gpio_sysfs_value[value], strlen(gpio_sysfs_value[value]));

	close(fd);

	if (rc < 0) {
		log_error("Could not write to '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	return 0;
}

int gpio_sysfs_get_input(GPIOSYSFS *gpio, GPIOSYSFSValue *value) {
	int fd;
	char buffer[GPIO_SYSFS_DIR_MAXLEN];
	char ret[3];

	snprintf(buffer, sizeof(buffer), "%s%s/value", GPIO_SYSFS_DIR, gpio->name);

	fd = open(buffer, O_RDONLY);
	if (fd < 0) {
		log_error("Could not open '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	if (read(fd, ret, 3) < 0) {
		log_error("Could not read from '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	if (ret[0] == '0') {
		*value = GPIO_SYSFS_VALUE_LOW;
	} else if (ret[0] == '1') {
		*value = GPIO_SYSFS_VALUE_HIGH;
	} else {
		log_error("Unknown value: %c", ret[0]);
		return -1;
	}

	return 0;
}

int gpio_sysfs_set_interrupt(GPIOSYSFS *gpio, GPIOSYSFSInterrupt interrupt) {
	int fd;
	char buffer[GPIO_SYSFS_DIR_MAXLEN];
	int rc;

    if(interrupt >= GPIO_SYSFS_INTERRUPT_NUM) {
		log_error("Unknown interrupt: %d", interrupt);
        return -1;
    }

	snprintf(buffer, sizeof(buffer), "%s%s/edge", GPIO_SYSFS_DIR, gpio->name);

	fd = open(buffer, O_WRONLY);

	if (fd < 0) {
		log_error("Could not open '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	rc = robust_write(fd, gpio_sysfs_interrupt[interrupt], strlen(gpio_sysfs_interrupt[interrupt]));

	close(fd);

	if (rc < 0) {
		log_error("Could not write to '%s': %s (%d)", buffer, get_errno_name(errno), errno);
		return -1;
	}

	return 0;
}

int gpio_sysfs_get_input_fd(GPIOSYSFS *gpio) {
	char buffer[GPIO_SYSFS_DIR_MAXLEN];

	snprintf(buffer, sizeof(buffer), "%s%s/value", GPIO_SYSFS_DIR, gpio->name);

	return open(buffer, O_RDONLY | O_NONBLOCK);
}
