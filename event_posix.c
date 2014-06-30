/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * event_posix.c: poll based event loop
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

#include "event.h"

#include "array.h"
#include "log.h"
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_EVENT

static Array _pollfds;

int event_init_platform(void) {
	// create pollfd array
	if (array_create(&_pollfds, 32, sizeof(struct pollfd), 1) < 0) {
		log_error("Could not create pollfd array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	return 0;
}

void event_exit_platform(void) {
	array_destroy(&_pollfds, NULL);
}

int event_source_added_platform(EventSource *event_source) {
	(void)event_source;

	return 0;
}

int event_source_modified_platform(EventSource *event_source) {
	(void)event_source;

	return 0;
}

void event_source_removed_platform(EventSource *event_source) {
	(void)event_source;
}

int event_run_platform(Array *event_sources, int *running, EventCleanupFunction cleanup) {
	int i;
	EventSource *event_source;
	struct pollfd *pollfd;
	int ready;
	int handled;

	*running = 1;

	cleanup();
	event_cleanup_sources();

	while (*running) {
		// update pollfd array
		if (array_resize(&_pollfds, event_sources->count, NULL) < 0) {
			log_error("Could not resize pollfd array: %s (%d)",
			          get_errno_name(errno), errno);

			return -1;
		}

		for (i = 0; i < event_sources->count; ++i) {
			event_source = array_get(event_sources, i);
			pollfd = array_get(&_pollfds, i);

			pollfd->fd = event_source->handle;
			pollfd->events = event_source->events;
			pollfd->revents = 0;
		}

		// start to poll
		log_debug("Starting to poll on %d event source(s)", _pollfds.count);

		ready = poll((struct pollfd *)_pollfds.bytes, _pollfds.count, -1);

		if (ready < 0) {
			if (errno_interrupted()) {
				log_debug("Poll got interrupted");

				continue;
			}

			log_error("Count not poll on event source(s): %s (%d)",
			          get_errno_name(errno), errno);

			*running = 0;

			return -1;
		}

		// handle poll result
		log_debug("Poll returned %d event source(s) as ready", ready);

		handled = 0;

		// this loop assumes that event source array and pollfd array can be
		// matched by index. this means that the first N items of the event
		// source array (with N = items in pollfd array) are not removed
		// or replaced during the iteration over the pollfd array. because
		// of this event_remove_source only marks event sources as removed,
		// the actual removal is done after this loop by event_cleanup_sources
		for (i = 0; i < _pollfds.count && ready > handled; ++i) {
			pollfd = array_get(&_pollfds, i);

			if (pollfd->revents == 0) {
				continue;
			}

			event_handle_source(array_get(event_sources, i), pollfd->revents);

			++handled;

			if (!*running) {
				break;
			}
		}

		if (ready == handled) {
			log_debug("Handled all ready event sources");
		} else {
			log_warn("Handled only %d of %d ready event source(s)",
			         handled, ready);
		}

		// now cleanup event sources that got marked as disconnected/removed
		// during the event handling
		cleanup();
		event_cleanup_sources();
	}

	return 0;
}

int event_stop_platform(void) {
	// nothing to do, the signal pipe already interrupted the running poll
	return 0;
}
