/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * pipe.h: Pipe specific functions
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

#ifndef DAEMONLIB_PIPE_H
#define DAEMONLIB_PIPE_H

#include <stdbool.h>

#include "io.h"

typedef struct {
	IOHandle read_end;
	IOHandle write_end;
} Pipe;

int pipe_create(Pipe *pipe, bool non_blocking);
void pipe_destroy(Pipe *pipe);

int pipe_read(Pipe *pipe, void *buffer, int length);
int pipe_write(Pipe *pipe, void *buffer, int length);

#endif // DAEMONLIB_PIPE_H
