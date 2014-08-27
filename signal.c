/*
 * daemonlib
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * signal.c: Signal specific functions
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

#include "signal.h"

#include "event.h"
#include "log.h"
#include "pipe.h"
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_EVENT

static Pipe _signal_pipe;
static SIGUSR1Function _handle_sigusr1 = NULL;

static void signal_handle(void *opaque) {
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

static void signal_forward(int signal_number) {
	pipe_write(&_signal_pipe, &signal_number, sizeof(signal_number));
}

int signal_init(SIGUSR1Function sigusr1) {
	int phase = 0;

	_handle_sigusr1 = sigusr1;

	// create signal pipe
	if (pipe_create(&_signal_pipe, 0) < 0) {
		log_error("Could not create signal pipe: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	if (event_add_source(_signal_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, signal_handle, NULL) < 0) {
		goto cleanup;
	}

	phase = 2;

	// handle SIGINT to stop the event loop
	if (signal(SIGINT, signal_forward) == SIG_ERR) {
		log_error("Could not install signal handler for SIGINT: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	// handle SIGTERM to stop the event loop
	if (signal(SIGTERM, signal_forward) == SIG_ERR) {
		log_error("Could not install signal handler for SIGTERM: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 4;

	// ignore SIGPIPE to make socket functions report EPIPE in case of broken pipes
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		log_error("Could not ignore SIGPIPE signal: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 5;

	// handle SIGUSR1 to call a user provided function
	if (signal(SIGUSR1, signal_forward) == SIG_ERR) {
		log_error("Could not install signal handler for SIGUSR1: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 6;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		signal(SIGPIPE, SIG_DFL);

	case 4:
		signal(SIGTERM, SIG_DFL);

	case 3:
		signal(SIGINT, SIG_DFL);

	case 2:
		event_remove_source(_signal_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC);

	case 1:
		pipe_destroy(&_signal_pipe);

	default:
		break;
	}

	return phase == 6 ? 0 : -1;
}

void signal_exit(void) {
	signal(SIGUSR1, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);

	event_remove_source(_signal_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC);
	pipe_destroy(&_signal_pipe);
}
