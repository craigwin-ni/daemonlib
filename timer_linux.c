/*
 * daemonlib
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * timer_linux.c: timerfd based timer implementation for Linux
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
#include <sys/timerfd.h>
#include <unistd.h>

#include "timer_linux.h"

#include "event.h"
#include "log.h"
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_OTHER

static void timer_handle_read(void *opaque) {
	Timer *timer = opaque;
	uint64_t value;
	int rc;

	do {
		rc = read(timer->handle, &value, sizeof(value));
	} while (rc < 0 && errno_interrupted());

	if (rc < 0) {
		if (errno_would_block()) {
			return;
		}

		log_error("Could not read from timerfd (handle: %d): %s (%d)",
		          timer->handle, get_errno_name(errno), errno);

		return;
	}

	timer->function(timer->opaque);
}

int timer_create_(Timer *timer, TimerFunction function, void *opaque) {
	timer->handle = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	if (timer->handle < 0) {
		log_error("Could not create timerfd: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	timer->function = function;
	timer->opaque = opaque;

	if (event_add_source(timer->handle, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, timer_handle_read, timer) < 0) {
		close(timer->handle);

		return -1;
	}

	log_debug("Created timerfd (handle: %d)", timer->handle);

	return 0;
}

void timer_destroy(Timer *timer) {
	log_debug("Destroying timerfd (handle: %d)", timer->handle);

	event_remove_source(timer->handle, EVENT_SOURCE_TYPE_GENERIC);

	close(timer->handle);
}

int timer_configure(Timer *timer, uint64_t delay, uint64_t interval) { // microseconds
	struct itimerspec itimerspec;

	itimerspec.it_value.tv_sec = delay / 1000000;
	itimerspec.it_value.tv_nsec = (delay % 1000000) * 1000;
	itimerspec.it_interval.tv_sec = interval / 1000000;
	itimerspec.it_interval.tv_nsec = (interval % 1000000) * 1000;

	if (timerfd_settime(timer->handle, 0, &itimerspec, NULL) < 0) {
		log_error("Could not configure timerfd (handle: %d): %s (%d)",
		          timer->handle, get_errno_name(errno), errno);

		return -1;
	}

	return 0;
}
