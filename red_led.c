/*
 * daemonlib
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * red_led.c: LED functions for RED Brick
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

#include <stdio.h>
#include <string.h>

#include "red_led.h"
#include "log.h"

#define LOG_CATEGORY LOG_CATEGORY_OTHER


#define LED_TRIGGER_NUM 6
#define LED_TRIGGER_STR_MAX_LENGTH 11
#define LED_PATH_STR_MAX_LENGTH 42

#define LED_TRIGGER_MAX_LENGTH 1024


static const char trigger_str[][LED_TRIGGER_STR_MAX_LENGTH] = {
	"cpu0",
	"gpio",
	"heartbeat",
	"mmc0",
	"none",
	"default-on"
};

static const char led_path[][LED_PATH_STR_MAX_LENGTH] = {
	"/sys/class/leds/pc05:green:status/trigger",
	"/sys/class/leds/pc06:red:error/trigger"
};

int led_set_trigger(LED led, LEDTrigger trigger) {
	FILE *f;
	int length;

	if(!((trigger >= LED_TRIGGER_CPU) && (trigger <= LED_TRIGGER_ON))) {
		log_error("Unknown LED trigger: %d (must be in [%d, %d])",
		          trigger,
		          LED_TRIGGER_CPU,
		          LED_TRIGGER_ON);
		return -1;
	}

	if(led > LED_RED) {
		log_error("Unknown LED: %d (must be in [%d, %d])",
		          led,
		          LED_GREEN,
		          LED_RED);
		return -1;
	}

    if((f = fopen(led_path[led], "w")) == NULL) {
		log_error("Could not open file %s", led_path[led]);
        return -1;
    }

    if((length = fprintf(f, "%s\n", trigger_str[trigger])) < ((int)strlen(trigger_str[trigger]))) {
		fclose(f);
		log_error("Could not write to file %s", led_path[led]);
		return -1;
	}

    if(fclose(f) < 0) {
		log_error("Could not close file %s", led_path[led]);
		return -1;
	}

    return 0;
}

LEDTrigger led_get_trigger(LED led) {
	char buf[LED_TRIGGER_MAX_LENGTH+1] = {0};
	FILE *f;
	int length;
	int i;

	if(led > LED_RED) {
		log_error("Unknown LED: %d (must be in [%d, %d])",
		          led,
		          LED_GREEN,
		          LED_RED);
		return -1;
	}

    if((f = fopen(led_path[led], "r")) == NULL) {
		log_error("Could not open file %s", led_path[led]);
        return LED_TRIGGER_ERROR;
    }

    if((length = fread(buf, sizeof(char), LED_TRIGGER_MAX_LENGTH, f)) <= 0) {
		fclose(f);
		log_error("Could not read from file %s", led_path[led]);
		return LED_TRIGGER_ERROR;
	}

	buf[length] = '\0';

	if(fclose(f) < 0) {
		log_error("Could not close file %s", led_path[led]);
		return LED_TRIGGER_ERROR;
	}

	char *start = strchr(buf, '[');
	char *end = strchr(buf, ']');

	if(start == NULL || end == NULL || start >= end) {
		return LED_TRIGGER_UNKNOWN;
	}

	++start; // skip '['
	*end = '\0'; // overwrite ']'
	for(i = 0; i < LED_TRIGGER_NUM; i++) {
		if(strcmp(start, trigger_str[i]) == 0) {
			return i;
		}
	}

    return LED_TRIGGER_UNKNOWN;
}

