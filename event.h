/*
 * daemonlib
 * Copyright (C) 2012, 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * event.h: Event specific functions
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

#ifndef DAEMONLIB_EVENT_H
#define DAEMONLIB_EVENT_H

#include <stdint.h>
#ifndef _WIN32
	#ifdef __linux__
		#include <sys/epoll.h>
	#else
		#include <poll.h>
	#endif
#endif

#include "io.h"

typedef void (*EventFunction)(void *opaque);
typedef void (*EventSIGUSR1Function)(void);
typedef void (*EventCleanupFunction)(void);

typedef enum {
#ifdef _WIN32
	EVENT_READ = 1 << 0,
	EVENT_WRITE = 1 << 2
#else
	#ifdef __linux__
		EVENT_READ = EPOLLIN,
		EVENT_WRITE = EPOLLOUT
	#else
		EVENT_READ = POLLIN,
		EVENT_WRITE = POLLOUT
	#endif
#endif
} Event;

typedef enum {
	EVENT_SOURCE_TYPE_GENERIC = 0,
	EVENT_SOURCE_TYPE_USB
} EventSourceType;

typedef enum {
	EVENT_SOURCE_STATE_NORMAL = 0,
	EVENT_SOURCE_STATE_ADDED,
	EVENT_SOURCE_STATE_REMOVED,
	EVENT_SOURCE_STATE_READDED,
	EVENT_SOURCE_STATE_MODIFIED
} EventSourceState;

typedef struct {
	IOHandle handle;
	EventSourceType type;
	uint32_t events;
	EventSourceState state;
	EventFunction read;
	void *read_opaque;
	EventFunction write;
	void *write_opaque;
} EventSource;

const char *event_get_source_type_name(EventSourceType type, int upper);

int event_init(EventSIGUSR1Function sigusr1);
void event_exit(void);

int event_add_source(IOHandle handle, EventSourceType type, uint32_t events,
                     EventFunction function, void *opaque);
int event_modify_source(IOHandle handle, EventSourceType type, uint32_t events_to_remove,
                        uint32_t events_to_add, EventFunction function, void *opaque);
void event_remove_source(IOHandle handle, EventSourceType type);
void event_cleanup_sources(void);

void event_handle_source(EventSource *event_source, uint32_t received_events);

int event_run(EventCleanupFunction cleanup);
void event_stop(void);

#endif // DAEMONLIB_EVENT_H
