/*
 * daemonlib
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 *
 * red_gpio.h: GPIO functions for RED Brick
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

#ifndef DAEMONLIB_RED_GPIO_H
#define DAEMONLIB_RED_GPIO_H

#include <stdint.h>

typedef enum {
	GPIO_PIN_0 = 0,
	GPIO_PIN_1 = 1,
	GPIO_PIN_2 = 2,
	GPIO_PIN_3 = 3,
	GPIO_PIN_4 = 4,
	GPIO_PIN_5 = 5,
	GPIO_PIN_6 = 6,
	GPIO_PIN_7 = 7,
	GPIO_PIN_8 = 8,
	GPIO_PIN_9 = 9,
	GPIO_PIN_10 = 10,
	GPIO_PIN_11 = 11,
	GPIO_PIN_12 = 12,
	GPIO_PIN_13 = 13,
	GPIO_PIN_14 = 14,
	GPIO_PIN_15 = 15,
	GPIO_PIN_16 = 16,
	GPIO_PIN_17 = 17,
	GPIO_PIN_18 = 18,
	GPIO_PIN_19 = 19,
	GPIO_PIN_20 = 20,
	GPIO_PIN_21 = 21,
	GPIO_PIN_22 = 22,
	GPIO_PIN_23 = 23,
	GPIO_PIN_24 = 24,
	GPIO_PIN_25 = 25,
	GPIO_PIN_26 = 26,
	GPIO_PIN_27 = 27,
	GPIO_PIN_28 = 28,
	GPIO_PIN_29 = 29,
	GPIO_PIN_30 = 30,
	GPIO_PIN_31 = 31
} GPIOPinIndex;

typedef enum {
	GPIO_PORT_A = 0,
	GPIO_PORT_B = 1,
	GPIO_PORT_C = 2,
	GPIO_PORT_D = 3,
	GPIO_PORT_E = 4,
	GPIO_PORT_F = 5,
	GPIO_PORT_G = 6,
	GPIO_PORT_H = 7,
	GPIO_PORT_I = 8,
} GPIOPortIndex;

typedef enum {
	GPIO_INPUT_DEFAULT = 0,
	GPIO_INPUT_PULLUP = 1,
	GPIO_INPUT_PULLDOWN = 2
} GPIOInputConfig;

typedef enum {
	GPIO_MUX_INPUT = 0,
	GPIO_MUX_OUTPUT = 1,
	GPIO_MUX_0 = 0,
	GPIO_MUX_1 = 1,
	GPIO_MUX_2 = 2,
	GPIO_MUX_3 = 3,
	GPIO_MUX_4 = 4,
	GPIO_MUX_5 = 5,
	GPIO_MUX_6 = 6,
} GPIOMux;

typedef struct {
	uint32_t config[4];
	uint32_t value;
	uint32_t multi_drive[2];
	uint32_t pull[2];
} GPIOPort;

typedef struct {
	GPIOPortIndex port_index;
	GPIOPinIndex pin_index;
} GPIOPin;

int gpio_init(void);
void gpio_mux_configure(const GPIOPin pin, const GPIOMux mux_config);
void gpio_input_configure(const GPIOPin pin, const GPIOInputConfig input_config);
void gpio_output_set(const GPIOPin pin);
void gpio_output_clear(const GPIOPin pin);
uint32_t gpio_input(const GPIOPin pin);
int gpio_sysfs_export(int gpio_num);
int gpio_sysfs_unexport(int gpio_num);
int gpio_sysfs_get_value_fd(char *gpio_name);

#endif // DAEMONLIB_RED_GPIO_H
