/*
 * daemonlib
 * Copyright (C) 2021 Matthias Bolte <matthias@tinkerforge.com>
 *
 * fifo.h: FIFO specific functions
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

#ifndef DAEMONLIB_FIFO_H
#define DAEMONLIB_FIFO_H

#include <stdbool.h>
#include <stdint.h>

#include "threads.h"

typedef enum { // bitmask
	FIFO_FLAG_NON_BLOCKING = 0x0001
} FIFOFlag;

typedef struct {
	Mutex mutex;
	Condition writable_condition;
	Condition readable_condition;
	void *buffer;
	int length;
	int begin; // inclusive
	int end; // exclusive
	bool shutdown;
} FIFO;

void fifo_create(FIFO *fifo, void *buffer, int length);
void fifo_destroy(FIFO *fifo);

int fifo_write(FIFO *fifo, const void *buffer, int length, uint32_t flags);
int fifo_read(FIFO *fifo, void *buffer, int length, uint32_t flags);

void fifo_shutdown(FIFO *fifo);

#endif // DAEMONLIB_FIFO_H
