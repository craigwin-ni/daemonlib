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
#include <stdio.h>

#include "macros.h"

#define ERRNO_WINAPI_OFFSET 71000000
#define ERRNO_ADDRINFO_OFFSET 72000000

#define BASE58_MAX_LENGTH 8

typedef void (*ItemDestroyFunction)(void *item);

typedef struct _Node Node;

struct _Node {
	Node *prev;
	Node *next;
};

bool errno_interrupted(void);
bool errno_would_block(void);

const char *get_errno_name(int error_code);

void string_copy(char *target, int target_length, const char *source);
void string_append(char *target, int target_length, const char *source);

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

void node_reset(Node *node);
void node_insert_before(Node *node, Node *insert);
void node_insert_after(Node *node, Node *insert);
void node_remove(Node *node);

int robust_read(int fd, void *buffer, int length);
int robust_write(int fd, const void *buffer, int length);

int robust_fread(FILE *fp, void *buffer, int length);
int robust_fwrite(FILE *fp, const void *buffer, int length);

int robust_snprintf(char *buffer, int length, const char *format, ...) ATTRIBUTE_FMT_PRINTF(3, 4);

#endif // DAEMONLIB_UTILS_H
