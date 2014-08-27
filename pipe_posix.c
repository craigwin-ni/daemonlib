/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * pipe_posix.c: POSIX based pipe implementation
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

/*
 * pipes are used to inject events into the poll based event loop. this
 * implementation is a direct wrapper of the POSIX pipe function.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "pipe.h"

#include "utils.h"

// sets errno on error
int pipe_create(Pipe *pipe_, uint32_t flags) {
	IOHandle handles[2];
	int fcntl_flags;
	int saved_errno;

	if (pipe(handles) < 0) {
		return -1;
	}

	pipe_->read_end = handles[0];
	pipe_->write_end = handles[1];

	if ((flags & PIPE_FLAG_NON_BLOCKING_READ) != 0) {
		fcntl_flags = fcntl(pipe_->read_end, F_GETFL, 0);

		if (fcntl_flags < 0 ||
		    fcntl(pipe_->read_end, F_SETFL, fcntl_flags | O_NONBLOCK) < 0) {
			goto error;
		}
	}

	if ((flags & PIPE_FLAG_NON_BLOCKING_WRITE) != 0) {
		fcntl_flags = fcntl(pipe_->write_end, F_GETFL, 0);

		if (fcntl_flags < 0 ||
		    fcntl(pipe_->write_end, F_SETFL, fcntl_flags | O_NONBLOCK) < 0) {
			goto error;
		}
	}

	return 0;

error:
	saved_errno = errno;

	close(pipe_->read_end);
	close(pipe_->write_end);

	errno = saved_errno;

	return -1;
}

void pipe_destroy(Pipe *pipe) {
	close(pipe->read_end);
	close(pipe->write_end);
}

// sets errno on error
int pipe_read(Pipe *pipe, void *buffer, int length) {
	int rc;

	// FIXME: handle partial read
	do {
		rc = read(pipe->read_end, buffer, length);
	} while (rc < 0 && errno_interrupted());

	return rc;
}

// sets errno on error
int pipe_write(Pipe *pipe, void *buffer, int length) {
	int rc;

	// FIXME: handle partial write
	do {
		rc = write(pipe->write_end, buffer, length);
	} while (rc < 0 && errno_interrupted());

	return rc;
}
