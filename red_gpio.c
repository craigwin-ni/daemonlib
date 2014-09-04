/*
 * daemonlib
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 *
 * red_gpio.c: GPIO functions for RED Brick
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

// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// sysconf
#include <unistd.h>

// mmap
#include <sys/mman.h>

#include "red_gpio.h"

#include "log.h"
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_OTHER
#define GPIO_BASE 0x01c20800
#define SYSFS_GPIO_DIR "/sys/class/gpio/"

static volatile GPIOPort *gpio_port;

int gpio_init(void) {
	int fd;
	uint32_t address_start, address_offset;
	uint32_t page_size, page_mask;
	void *mapped_base;

	fd = open("/dev/mem", O_RDWR);

	if (fd < 0) {
		log_error("Could not open '/dev/mem': %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	page_size = sysconf(_SC_PAGESIZE);
	page_mask = ~(page_size - 1);

	address_start  = GPIO_BASE &  page_mask;
	address_offset = GPIO_BASE & ~page_mask;

	// FIXME: add gpio_exit() to munmap?
	mapped_base = mmap(0,
	                   page_size * 2,
	                   PROT_READ | PROT_WRITE,
	                   MAP_SHARED,
	                   fd,
	                   address_start);

	if (mapped_base == MAP_FAILED) {
		log_error("Could not mmap '/dev/mem': %s (%d)",
		          get_errno_name(errno), errno);

		close(fd);

		return -1;
	}

	gpio_port = mapped_base + address_offset;

	close(fd);

	return 0;
}

void gpio_mux_configure(const GPIOPin pin, const GPIOMux mux_config) {
	uint32_t config_index =  pin.pin_index        >> 3;
	uint32_t offset       = (pin.pin_index & 0x7) << 2;
	uint32_t config       = gpio_port[pin.port_index].config[config_index];

	config &= ~(0xF << offset);
	config |= mux_config << offset;

	gpio_port[pin.port_index].config[config_index] = config;
}

void gpio_output_set(const GPIOPin pin) {
	gpio_port[pin.port_index].value |= (1 << pin.pin_index);
}

void gpio_output_clear(const GPIOPin pin) {
	gpio_port[pin.port_index].value &= ~(1 << pin.pin_index);
}

uint32_t gpio_input(const GPIOPin pin) {
	return gpio_port[pin.port_index].value & (1 << pin.pin_index);
}


// sysfs operations, gpio_num and gpio_name are defined in fex file
int gpio_sysfs_export(int gpio_num) {
	int fd, len;
	char buf[32];

	fd = open(SYSFS_GPIO_DIR "export", O_WRONLY);
	if(fd < 0) {
		return fd;
	}

	len = snprintf(buf, sizeof(buf), "%d", gpio_num);
	write(fd, buf, len);
	close(fd);

	return 0;
}

int gpio_sysfs_unexport(int gpio_num) {
	int fd, len;
	char buf[32];

	fd = open(SYSFS_GPIO_DIR "unexport", O_WRONLY);
	if(fd < 0) {
		return fd;
	}

	len = snprintf(buf, sizeof(buf), "%d", gpio_num);
	write(fd, buf, len);
	close(fd);

	return 0;
}

int gpio_sysfs_get_value_fd(char *gpio_name) {
	int fd;
	char buf[1024];

	snprintf(buf, sizeof(buf), "%s%s%s", SYSFS_GPIO_DIR, gpio_name, "/value");

	return open(buf, O_RDONLY | O_NONBLOCK );
}
