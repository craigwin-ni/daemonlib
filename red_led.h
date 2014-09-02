/*
 * daemonlib
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * red_led.h: LED functions for RED Brick
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
	LED_GREEN = 0,
	LED_RED   = 1
} LED;

typedef enum {
	LED_TRIGGER_CPU       =  0, 
	LED_TRIGGER_GPIO      =  1,
	LED_TRIGGER_HEARTBEAT =  2,
	LED_TRIGGER_MMC       =  3,
	LED_TRIGGER_OFF       =  4,
	LED_TRIGGER_ON        =  5,

	LED_TRIGGER_UNKNOWN   =  -1,
	LED_TRIGGER_ERROR     =  -2
} LEDTrigger;

int led_set_trigger(LED led, LEDTrigger trigger);
LEDTrigger led_get_trigger(LED led);

#endif // DAEMONLIB_RED_LED_H

