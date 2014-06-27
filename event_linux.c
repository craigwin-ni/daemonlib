/*
 * daemonlib
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * event_linux.c: epoll based event loop
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
#include <signal.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "event.h"

#include "array.h"
#include "log.h"
#include "pipe.h"
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_EVENT

static int _epollfd = -1;
static int _epollfd_event_count = 0;
static Pipe _signal_pipe;
static EventSIGUSR1Function _handle_sigusr1 = NULL;

static void event_handle_signal(void *opaque) {
	int signal_number;

	(void)opaque;

	if (pipe_read(&_signal_pipe, &signal_number, sizeof(signal_number)) < 0) {
		log_error("Could not read from signal pipe: %s (%d)",
		          get_errno_name(errno), errno);

		return;
	}

	if (signal_number == SIGINT) {
		log_info("Received SIGINT");

		event_stop();
	} else if (signal_number == SIGTERM) {
		log_info("Received SIGTERM");

		event_stop();
	} else if (signal_number == SIGUSR1) {
		log_info("Received SIGUSR1");

		if (_handle_sigusr1 != NULL) {
			_handle_sigusr1();
		}
	} else {
		log_warn("Received unexpected signal %d", signal_number);
	}
}

static void event_forward_signal(int signal_number) {
	pipe_write(&_signal_pipe, &signal_number, sizeof(signal_number));
}

int event_init_platform(EventSIGUSR1Function sigusr1) {
	int phase = 0;

	_handle_sigusr1 = sigusr1;

	// create epollfd
	_epollfd = epoll_create1(EPOLL_CLOEXEC);

	if (_epollfd < 0) {
		log_error("Could not create epollfd: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	// create signal pipe
	if (pipe_create(&_signal_pipe) < 0) {
		log_error("Could not create signal pipe: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	if (event_add_source(_signal_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, event_handle_signal, NULL) < 0) {
		goto cleanup;
	}

	phase = 3;

	// setup signal handlers
	if (signal(SIGINT, event_forward_signal) == SIG_ERR) {
		log_error("Could not install signal handler for SIGINT: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 4;

	if (signal(SIGTERM, event_forward_signal) == SIG_ERR) {
		log_error("Could not install signal handler for SIGTERM: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 5;

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		log_error("Could not ignore SIGPIPE signal: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 6;

	if (signal(SIGUSR1, event_forward_signal) == SIG_ERR) {
		log_error("Could not install signal handler for SIGUSR1: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 7;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 6:
		signal(SIGPIPE, SIG_DFL);

	case 5:
		signal(SIGTERM, SIG_DFL);

	case 4:
		signal(SIGINT, SIG_DFL);

	case 3:
		event_remove_source(_signal_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC);

	case 2:
		pipe_destroy(&_signal_pipe);

	case 1:
		close(_epollfd);

	default:
		break;
	}

	return phase == 7 ? 0 : -1;
}

void event_exit_platform(void) {
	signal(SIGUSR1, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);

	event_remove_source(_signal_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC);
	pipe_destroy(&_signal_pipe);

	// FIXME: remove events from epollfd?

	close(_epollfd);
}

int event_source_added_platform(EventSource *event_source) {
	struct epoll_event event;

	event.events = event_source->events;
	event.data.ptr = event_source;

	if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, event_source->handle, &event) < 0) {
		log_error("Could not add %s event source (handle: %d) to epollfd: %s (%d)",
		          event_get_source_type_name(event_source->type, 0),
		          event_source->handle, get_errno_name(errno), errno);

		return -1;
	}

	++_epollfd_event_count;

	return 0;
}

int event_source_modified_platform(EventSource *event_source) {
	struct epoll_event event;

	event.events = event_source->events;
	event.data.ptr = event_source;

	if (epoll_ctl(_epollfd, EPOLL_CTL_MOD, event_source->handle, &event) < 0) {
		log_error("Could not modify %s event source (handle: %d) added to epollfd: %s (%d)",
		          event_get_source_type_name(event_source->type, 0),
		          event_source->handle, get_errno_name(errno), errno);

		return -1;
	}

	return 0;
}

void event_source_removed_platform(EventSource *event_source) {
	struct epoll_event event;

	event.events = event_source->events;
	event.data.ptr = event_source;

	if (epoll_ctl(_epollfd, EPOLL_CTL_DEL, event_source->handle, &event) < 0) {
		log_error("Could not remove %s event source (handle: %d) from epollfd: %s (%d)",
		          event_get_source_type_name(event_source->type, 0),
		          event_source->handle, get_errno_name(errno), errno);

		return;
	}

	--_epollfd_event_count;
}

int event_run_platform(Array *event_sources, int *running, EventCleanupFunction cleanup) {
	int i;
	EventSource *event_source;
	Array received_events;
	struct epoll_event *received_event;
	int ready;

	(void)event_sources;

	if (array_create(&received_events, 32, sizeof(struct epoll_event), 1) < 0) {
		log_error("Could not create epoll event array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	*running = 1;

	cleanup();
	event_cleanup_sources();

	while (*running) {
		if (array_resize(&received_events, _epollfd_event_count, NULL) < 0) {
			log_error("Could not resize pollfd array: %s (%d)",
			          get_errno_name(errno), errno);

			return -1;
		}

		// start to epoll
		log_debug("Starting to epoll on %d event source(s)", _epollfd_event_count);

		ready = epoll_wait(_epollfd, (struct epoll_event *)received_events.bytes,
		                   received_events.count, -1);

		if (ready < 0) {
			if (errno_interrupted()) {
				log_debug("EPoll got interrupted");

				continue;
			}

			log_error("Count not epoll on event source(s): %s (%d)",
			          get_errno_name(errno), errno);

			*running = 0;

			return -1;
		}

		// handle poll result
		log_debug("EPoll returned %d event source(s) as ready", ready);

		// this loop assumes that event source array and pollfd array can be
		// matched by index. this means that the first N items of the event
		// source array (with N = items in pollfd array) are not removed
		// or replaced during the iteration over the pollfd array. because
		// of this event_remove_source only marks event sources as removed,
		// the actual removal is done after this loop by event_cleanup_sources
		for (i = 0; i < ready; ++i) {
			received_event = array_get(&received_events, i);
			event_source = received_event->data.ptr;

			event_handle_source(event_source, received_event->events);

			if (!*running) {
				break;
			}
		}

		log_debug("Handled all ready event sources");

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
