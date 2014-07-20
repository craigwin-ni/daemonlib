/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 *
 * utils.h: Utility functions
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

#ifndef DAEMONLIB_UTILS_H
#define DAEMONLIB_UTILS_H

#include <stdbool.h>
#include <stdint.h>

#define ERRNO_WINAPI_OFFSET 71000000
#define ERRNO_ADDRINFO_OFFSET 72000000

#define BASE58_MAX_LENGTH 8

typedef void (*ItemDestroyFunction)(void *item);

bool errno_interrupted(void);
bool errno_would_block(void);

const char *get_errno_name(int error_code);

// round SIZE up to the next multiple of 16. the calculation relies on SIZE
// being a signed int. with float the division would not truncate the result.
// with unsigned int SIZE - 1 would overflow to a big value if size is 0.
#define GROW_ALLOCATION(size) ((((int)(size) - 1) / 16 + 1) * 16)

void string_copy(char *destination, const char *source, int destination_length);
void string_append(char *destination, const char *source, int destination_length);

char *base58_encode(char *base58, uint32_t value);
int base58_decode(uint32_t *value, const char *base58);

uint16_t uint16_to_le(uint16_t native);
uint32_t uint32_to_le(uint32_t native);

uint32_t uint32_from_le(uint32_t value);

void millisleep(uint32_t milliseconds);

uint64_t microseconds(void);

#if !defined _GNU_SOURCE && !defined __APPLE__
char *strcasestr(char *haystack, char *needle);
#endif

int red_brick_uid(uint32_t *uid /* always little endian */);

#endif // DAEMONLIB_UTILS_H
