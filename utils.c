/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 * Copyright (C) 2014 Olaf Lüke <olaf@tinkerforge.com>
 *
 * utils.c: Utility functions
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _MSC_VER
	#include <sys/time.h>
#endif
#ifdef _WIN32
	#include <winsock2.h> // must be included before windows.h
	#include <windows.h>
	#include <io.h>
#else
	#include <netdb.h>
	#include <unistd.h>
#endif

#include "utils.h"

#include "base58.h"

#if !defined(_WIN32) && !defined(EAI_ADDRFAMILY)
	#if EAI_AGAIN < 0
		#define EAI_ADDRFAMILY -9
	#else
		#define EAI_ADDRFAMILY 9
	#endif
#endif

bool errno_interrupted(void) {
#ifdef _WIN32
	return errno == ERRNO_WINAPI_OFFSET + WSAEINTR;
#else
	return errno == EINTR;
#endif
}

bool errno_would_block(void) {
#ifdef _WIN32
	return errno == ERRNO_WINAPI_OFFSET + WSAEWOULDBLOCK;
#else
	return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}

const char *get_errno_name(int error_code) {
	#define ERRNO_NAME(code) case code: return #code
	#define WINAPI_ERROR_NAME(code) case ERRNO_WINAPI_OFFSET + code: return #code
#ifndef _WIN32
	#if EAI_AGAIN < 0
		#define ADDRINFO_ERROR_NAME(code) case ERRNO_ADDRINFO_OFFSET - code: return #code
	#else
		#define ADDRINFO_ERROR_NAME(code) case ERRNO_ADDRINFO_OFFSET + code: return #code
	#endif
#endif

	switch (error_code) {
	ERRNO_NAME(EPERM);
	ERRNO_NAME(ENOENT);
	ERRNO_NAME(ESRCH);
	ERRNO_NAME(EINTR);
	ERRNO_NAME(EIO);
	ERRNO_NAME(ENXIO);
	ERRNO_NAME(E2BIG);
	ERRNO_NAME(ENOEXEC);
	ERRNO_NAME(EBADF);
	ERRNO_NAME(ECHILD);
	ERRNO_NAME(EAGAIN);
	ERRNO_NAME(ENOMEM);
	ERRNO_NAME(EACCES);
	ERRNO_NAME(EFAULT);
#ifdef ENOTBLK
	ERRNO_NAME(ENOTBLK);
#endif
	ERRNO_NAME(EBUSY);
	ERRNO_NAME(EEXIST);
	ERRNO_NAME(EXDEV);
	ERRNO_NAME(ENODEV);
	ERRNO_NAME(ENOTDIR);
	ERRNO_NAME(EISDIR);
	ERRNO_NAME(EINVAL);
	ERRNO_NAME(ENFILE);
	ERRNO_NAME(EMFILE);
	ERRNO_NAME(ENOTTY);
#ifdef ETXTBSY
	ERRNO_NAME(ETXTBSY);
#endif
	ERRNO_NAME(EFBIG);
	ERRNO_NAME(ENOSPC);
	ERRNO_NAME(ESPIPE);
	ERRNO_NAME(EROFS);
	ERRNO_NAME(EMLINK);
	ERRNO_NAME(EPIPE);
	ERRNO_NAME(EDOM);
	ERRNO_NAME(ERANGE);
	ERRNO_NAME(EDEADLK);
	ERRNO_NAME(ENAMETOOLONG);
	ERRNO_NAME(ENOLCK);
	ERRNO_NAME(ENOSYS);
	ERRNO_NAME(ENOTEMPTY);

#ifndef _WIN32
	ERRNO_NAME(ENOTSUP);
	ERRNO_NAME(ELOOP);
	#if EWOULDBLOCK != EAGAIN
	ERRNO_NAME(EWOULDBLOCK);
	#endif
	ERRNO_NAME(ENOMSG);
	ERRNO_NAME(EIDRM);
	ERRNO_NAME(ENOSTR);
	ERRNO_NAME(ENODATA);
	ERRNO_NAME(ETIME);
	ERRNO_NAME(ENOSR);
	ERRNO_NAME(EREMOTE);
	ERRNO_NAME(ENOLINK);
	ERRNO_NAME(EPROTO);
	ERRNO_NAME(EMULTIHOP);
	ERRNO_NAME(EBADMSG);
	ERRNO_NAME(EOVERFLOW);
	ERRNO_NAME(EUSERS);
	ERRNO_NAME(ENOTSOCK);
	ERRNO_NAME(EDESTADDRREQ);
	ERRNO_NAME(EMSGSIZE);
	ERRNO_NAME(EPROTOTYPE);
	ERRNO_NAME(ENOPROTOOPT);
	ERRNO_NAME(EPROTONOSUPPORT);
	ERRNO_NAME(ESOCKTNOSUPPORT);
	#if EOPNOTSUPP != ENOTSUP
	ERRNO_NAME(EOPNOTSUPP);
	#endif
	ERRNO_NAME(EPFNOSUPPORT);
	ERRNO_NAME(EAFNOSUPPORT);
	ERRNO_NAME(EADDRINUSE);
	ERRNO_NAME(EADDRNOTAVAIL);
	ERRNO_NAME(ENETDOWN);
	ERRNO_NAME(ENETUNREACH);
	ERRNO_NAME(ENETRESET);
	ERRNO_NAME(ECONNABORTED);
	ERRNO_NAME(ECONNRESET);
	ERRNO_NAME(ENOBUFS);
	ERRNO_NAME(EISCONN);
	ERRNO_NAME(ENOTCONN);
	ERRNO_NAME(ESHUTDOWN);
	ERRNO_NAME(ETOOMANYREFS);
	ERRNO_NAME(ETIMEDOUT);
	ERRNO_NAME(ECONNREFUSED);
	ERRNO_NAME(EHOSTDOWN);
	ERRNO_NAME(EHOSTUNREACH);
	ERRNO_NAME(EALREADY);
	ERRNO_NAME(EINPROGRESS);
	ERRNO_NAME(ESTALE);
	ERRNO_NAME(EDQUOT);
	ERRNO_NAME(ECANCELED);
	ERRNO_NAME(EOWNERDEAD);
	ERRNO_NAME(ENOTRECOVERABLE);
#endif

#if !defined _WIN32 && !defined __APPLE__
	ERRNO_NAME(ECHRNG);
	ERRNO_NAME(EL2NSYNC);
	ERRNO_NAME(EL3HLT);
	ERRNO_NAME(EL3RST);
	ERRNO_NAME(ELNRNG);
	ERRNO_NAME(EUNATCH);
	ERRNO_NAME(ENOCSI);
	ERRNO_NAME(EL2HLT);
	ERRNO_NAME(EBADE);
	ERRNO_NAME(EBADR);
	ERRNO_NAME(EXFULL);
	ERRNO_NAME(ENOANO);
	ERRNO_NAME(EBADRQC);
	ERRNO_NAME(EBADSLT);
	#if EDEADLOCK != EDEADLK
	ERRNO_NAME(EDEADLOCK);
	#endif
	ERRNO_NAME(EBFONT);
	ERRNO_NAME(ENONET);
	ERRNO_NAME(ENOPKG);
	ERRNO_NAME(EADV);
	ERRNO_NAME(ESRMNT);
	ERRNO_NAME(ECOMM);
	ERRNO_NAME(EDOTDOT);
	ERRNO_NAME(ENOTUNIQ);
	ERRNO_NAME(EBADFD);
	ERRNO_NAME(EREMCHG);
	ERRNO_NAME(ELIBACC);
	ERRNO_NAME(ELIBBAD);
	ERRNO_NAME(ELIBSCN);
	ERRNO_NAME(ELIBMAX);
	ERRNO_NAME(ELIBEXEC);
	ERRNO_NAME(EILSEQ);
	ERRNO_NAME(ERESTART);
	ERRNO_NAME(ESTRPIPE);
	ERRNO_NAME(EUCLEAN);
	ERRNO_NAME(ENOTNAM);
	ERRNO_NAME(ENAVAIL);
	ERRNO_NAME(EISNAM);
	ERRNO_NAME(EREMOTEIO);
	ERRNO_NAME(ENOMEDIUM);
	ERRNO_NAME(EMEDIUMTYPE);
	ERRNO_NAME(ENOKEY);
	ERRNO_NAME(EKEYEXPIRED);
	ERRNO_NAME(EKEYREVOKED);
	ERRNO_NAME(EKEYREJECTED);
	#ifdef ERFKILL
	ERRNO_NAME(ERFKILL);
	#endif
#endif

#ifdef _WIN32
	WINAPI_ERROR_NAME(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT);
	WINAPI_ERROR_NAME(ERROR_INVALID_DATA);
	WINAPI_ERROR_NAME(ERROR_ACCESS_DENIED);
	WINAPI_ERROR_NAME(ERROR_INVALID_HANDLE);
	WINAPI_ERROR_NAME(ERROR_INVALID_NAME);
	WINAPI_ERROR_NAME(ERROR_CIRCULAR_DEPENDENCY);
	WINAPI_ERROR_NAME(ERROR_INVALID_PARAMETER);
	WINAPI_ERROR_NAME(ERROR_INVALID_SERVICE_ACCOUNT);
	WINAPI_ERROR_NAME(ERROR_DUPLICATE_SERVICE_NAME);
	WINAPI_ERROR_NAME(ERROR_SERVICE_ALREADY_RUNNING);
	WINAPI_ERROR_NAME(ERROR_SERVICE_DOES_NOT_EXIST);
	WINAPI_ERROR_NAME(ERROR_SERVICE_EXISTS);
	WINAPI_ERROR_NAME(ERROR_SERVICE_MARKED_FOR_DELETE);
	WINAPI_ERROR_NAME(ERROR_INSUFFICIENT_BUFFER);
	WINAPI_ERROR_NAME(ERROR_INVALID_WINDOW_HANDLE);
	WINAPI_ERROR_NAME(ERROR_ALREADY_EXISTS);
	WINAPI_ERROR_NAME(ERROR_FILE_NOT_FOUND);
	WINAPI_ERROR_NAME(ERROR_INVALID_SERVICE_CONTROL);
	WINAPI_ERROR_NAME(ERROR_OPERATION_ABORTED);
	WINAPI_ERROR_NAME(ERROR_IO_INCOMPLETE);
	WINAPI_ERROR_NAME(ERROR_IO_PENDING);
	WINAPI_ERROR_NAME(ERROR_PIPE_BUSY);

	WINAPI_ERROR_NAME(WSAEINTR);
	WINAPI_ERROR_NAME(WSAEBADF);
	WINAPI_ERROR_NAME(WSAEACCES);
	WINAPI_ERROR_NAME(WSAEFAULT);
	WINAPI_ERROR_NAME(WSAEINVAL);
	WINAPI_ERROR_NAME(WSAEMFILE);
	WINAPI_ERROR_NAME(WSAEWOULDBLOCK);
	WINAPI_ERROR_NAME(WSAEINPROGRESS);
	WINAPI_ERROR_NAME(WSAEALREADY);
	WINAPI_ERROR_NAME(WSAENOTSOCK);
	WINAPI_ERROR_NAME(WSAEDESTADDRREQ);
	WINAPI_ERROR_NAME(WSAEMSGSIZE);
	WINAPI_ERROR_NAME(WSAEPROTOTYPE);
	WINAPI_ERROR_NAME(WSAENOPROTOOPT);
	WINAPI_ERROR_NAME(WSAEPROTONOSUPPORT);
	WINAPI_ERROR_NAME(WSAESOCKTNOSUPPORT);
	WINAPI_ERROR_NAME(WSAEOPNOTSUPP);
	WINAPI_ERROR_NAME(WSAEPFNOSUPPORT);
	WINAPI_ERROR_NAME(WSAEAFNOSUPPORT);
	WINAPI_ERROR_NAME(WSAEADDRINUSE);
	WINAPI_ERROR_NAME(WSAEADDRNOTAVAIL);
	WINAPI_ERROR_NAME(WSAENETDOWN);
	WINAPI_ERROR_NAME(WSAENETUNREACH);
	WINAPI_ERROR_NAME(WSAENETRESET);
	WINAPI_ERROR_NAME(WSAECONNABORTED);
	WINAPI_ERROR_NAME(WSAECONNRESET);
	WINAPI_ERROR_NAME(WSAENOBUFS);
	WINAPI_ERROR_NAME(WSAEISCONN);
	WINAPI_ERROR_NAME(WSAENOTCONN);
	WINAPI_ERROR_NAME(WSAESHUTDOWN);
	WINAPI_ERROR_NAME(WSAETOOMANYREFS);
	WINAPI_ERROR_NAME(WSAETIMEDOUT);
	WINAPI_ERROR_NAME(WSAECONNREFUSED);
	WINAPI_ERROR_NAME(WSAELOOP);
	WINAPI_ERROR_NAME(WSAENAMETOOLONG);
	WINAPI_ERROR_NAME(WSAEHOSTDOWN);
	WINAPI_ERROR_NAME(WSAEHOSTUNREACH);
	WINAPI_ERROR_NAME(WSAENOTEMPTY);
	WINAPI_ERROR_NAME(WSAEPROCLIM);
	WINAPI_ERROR_NAME(WSAEUSERS);
	WINAPI_ERROR_NAME(WSAEDQUOT);
	WINAPI_ERROR_NAME(WSAESTALE);
	WINAPI_ERROR_NAME(WSAEREMOTE);

	WINAPI_ERROR_NAME(WSATRY_AGAIN);
	WINAPI_ERROR_NAME(WSANO_RECOVERY);
	WINAPI_ERROR_NAME(WSA_NOT_ENOUGH_MEMORY);
	WINAPI_ERROR_NAME(WSAHOST_NOT_FOUND);
#endif

#ifndef _WIN32
	ADDRINFO_ERROR_NAME(EAI_AGAIN);
	ADDRINFO_ERROR_NAME(EAI_BADFLAGS);
	ADDRINFO_ERROR_NAME(EAI_FAIL);
	ADDRINFO_ERROR_NAME(EAI_FAMILY);
	ADDRINFO_ERROR_NAME(EAI_MEMORY);
	ADDRINFO_ERROR_NAME(EAI_NONAME);
	ADDRINFO_ERROR_NAME(EAI_OVERFLOW);
	ADDRINFO_ERROR_NAME(EAI_SYSTEM);
	ADDRINFO_ERROR_NAME(EAI_ADDRFAMILY);
#endif

	// FIXME

	default: return "<unknown>";
	}

	#undef ERRNO_NAME
	#undef WINAPI_ERROR_NAME
	#undef ADDRINFO_ERROR_NAME
}

void string_copy(char *target, int target_length, const char *source) {
	if (target_length <= 0) {
		return;
	}

	strncpy(target, source, target_length - 1);

	target[target_length - 1] = '\0';
}

void string_append(char *target, int target_length, const char *source) {
	int offset;

	if (target_length <= 0) {
		return;
	}

	offset = strlen(target);

	if (offset >= target_length - 1) {
		return;
	}

	strncpy(target + offset, source, target_length - offset - 1);

	target[target_length - 1] = '\0';
}

// convert from host endian to little endian
uint16_t uint16_to_le(uint16_t native) {
	union {
		uint8_t bytes[2];
		uint16_t little;
	} c;

	c.bytes[0] = (native >> 0) & 0xFF;
	c.bytes[1] = (native >> 8) & 0xFF;

	return c.little;
}

// convert from host endian to little endian
uint32_t uint32_to_le(uint32_t native) {
	union {
		uint8_t bytes[4];
		uint32_t little;
	} c;

	c.bytes[0] = (native >>  0) & 0xFF;
	c.bytes[1] = (native >>  8) & 0xFF;
	c.bytes[2] = (native >> 16) & 0xFF;
	c.bytes[3] = (native >> 24) & 0xFF;

	return c.little;
}

// convert from little endian to host endian
uint32_t uint32_from_le(uint32_t value) {
	uint8_t *bytes = (uint8_t *)&value;

	return ((uint32_t)bytes[3] << 24) |
	       ((uint32_t)bytes[2] << 16) |
	       ((uint32_t)bytes[1] <<  8) |
	       ((uint32_t)bytes[0] <<  0);
}

void millisleep(uint32_t milliseconds) {
#ifdef _WIN32
	Sleep(milliseconds);
#else
	// FIXME: (u)sleep can be interrupted, might have to deal with that
	if (milliseconds >= 1000) {
		sleep(milliseconds / 1000);

		milliseconds %= 1000;
	}

	usleep(milliseconds * 1000);
#endif
}

uint64_t microseconds(void) {
#ifdef __linux__
	struct timespec ts;

	#ifdef CLOCK_MONOTONIC_RAW
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) < 0) {
	#else
	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
	#endif
		return 0;
	} else {
		return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
	}
#else
	struct timeval tv;

	// FIXME: use a monotonic source such as QueryPerformanceCounter() or mach_absolute_time()
	if (gettimeofday(&tv, NULL) < 0) {
		return 0;
	} else {
		return tv.tv_sec * 1000000 + tv.tv_usec;
	}
#endif
}

#if !defined _GNU_SOURCE && !defined __APPLE__

#include <ctype.h>

char *strcasestr(char *haystack, char *needle) {
	char *p, *startn = NULL, *np = NULL;

	for (p = haystack; *p != '\0'; ++p) {
		if (np != NULL) {
			if (toupper(*p) == toupper(*np)) {
				if (*++np == '\0') {
					return startn;
				}
			} else {
				np = NULL;
			}
		} else if (toupper(*p) == toupper(*needle)) {
			np = needle + 1;
			startn = p;
		}
	}

	return NULL;
}

#endif

// sets errno on error
int red_brick_uid(uint32_t *uid /* always little endian */) {
	FILE *fp;
	char base58[BASE58_MAX_LENGTH + 1]; // +1 for the \n
	int rc;
	int saved_errno;

	// read UID from /proc/red_brick_uid
	fp = fopen("/proc/red_brick_uid", "rb");

	if (fp == NULL) {
		return -1;
	}

	rc = fread(base58, 1, sizeof(base58), fp);
	saved_errno = errno;

	fclose(fp);

	errno = saved_errno;

	if (rc < 1) {
		return -1;
	}

	if (base58[rc - 1] != '\n') {
		errno = EINVAL;

		return -1;
	}

	base58[rc - 1] = '\0';

	if (base58_decode(uid, base58) < 0) {
		return -1;
	}

	*uid = uint32_to_le(*uid);

	return 0;
}

// sets errno on error
int robust_read(int fd, void *buffer, int length) {
	int rc;

	do {
		rc = read(fd, buffer, length);
	} while (rc < 0 && errno_interrupted());

	return rc;
}

// sets errno on error
int robust_write(int fd, const void *buffer, int length) {
	int rc;

	do {
		// FIXME: handle partial write
		rc = write(fd, buffer, length);
	} while (rc < 0 && errno_interrupted());

	return rc;
}

// sets errno on error
int robust_fread(FILE *fp, void *buffer, int length) {
	int rc;

	do {
		rc = fread(buffer, 1, length, fp);
	} while (rc == 0 && ferror(fp) && errno_interrupted());

	return rc == 0 && ferror(fp) ? -1 : rc;
}

// sets errno on error
int robust_fwrite(FILE *fp, const void *buffer, int length) {
	int rc;

	do {
		// FIXME: handle partial fwrite
		rc = fwrite(buffer, 1, length, fp);
	} while (rc == 0 && ferror(fp) && errno_interrupted());

	return rc == 0 && ferror(fp) ? -1 : rc;
}

// sets errno on error
int robust_snprintf(char *buffer, int length, const char *format, ...) {
	va_list arguments;
	int rc;

	va_start(arguments, format);

	errno = 0;
	rc = vsnprintf(buffer, length, format, arguments);

	if (rc < 0) {
		if (errno == 0) {
			errno = EINVAL;
		}

		rc = -1;
	} else if (rc >= length) {
		if (errno == 0) {
			errno = ERANGE;
		}

		rc = -1;
	} else {
		rc = 0;
	}

	va_end(arguments);

	return rc;
}
