/*
 * daemonlib
 * Copyright (C) 2014-2017 Matthias Bolte <matthias@tinkerforge.com>
 *
 * socket.c: Socket implementation
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
#include <stdlib.h>
#ifndef _WIN32
	#include <netdb.h>
	#include <unistd.h>
#endif

#include "socket.h"

#include "log.h"
#include "utils.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

extern void socket_destroy_platform(Socket *socket);
extern int socket_accept_platform(Socket *socket, Socket *accepted_socket,
                                  struct sockaddr *address, socklen_t *length);
extern int socket_listen_platform(Socket *socket, int backlog);
extern int socket_receive_platform(Socket *socket, void *buffer, int length);
extern int socket_send_platform(Socket *socket, const void *buffer, int length);

static const char *socket_get_address_family_name(int family, bool dual_stack) {
	switch (family) {
	case AF_INET:
		return "IPv4";

	case AF_INET6:
		return dual_stack ? "IPv6 dual-stack" : "IPv6";

	default:
		return "<unknown>";
	}
}

// sets errno on error
int socket_create(Socket *socket) {
	if (io_create(&socket->base, "plain-socket",
	              (IODestroyFunction)socket_destroy,
	              (IOReadFunction)socket_receive,
	              (IOWriteFunction)socket_send) < 0) {
		return -1;
	}

	socket->handle = IO_HANDLE_INVALID;
	socket->family = AF_UNSPEC;
	socket->create_allocated = NULL;
	socket->destroy = socket_destroy_platform;
	socket->receive = socket_receive_platform;
	socket->send = socket_send_platform;

	return 0;
}

// sets errno on error
Socket *socket_create_allocated(void) {
	Socket *socket = calloc(1, sizeof(Socket));

	if (socket == NULL) {
		errno = ENOMEM;

		return NULL;
	}

	if (socket_create(socket) < 0) {
		free(socket);

		return NULL;
	}

	return socket;
}

void socket_destroy(Socket *socket) {
	socket->destroy(socket);
}

// sets errno on error
Socket *socket_accept(Socket *socket, struct sockaddr *address, socklen_t *length) {
	int rc;
	Socket *allocated_socket = socket->create_allocated();

	if (allocated_socket == NULL) {
		// because accept() is not called now the event loop will receive
		// another event on the server socket to indicate the pending
		// connection attempt. but we're currently in an OOM situation so
		// there are other things to worry about.
		errno = ENOMEM;

		return NULL;
	}

	rc = socket_accept_platform(socket, allocated_socket, address, length);

	if (rc < 0) {
		free(allocated_socket);

		return NULL;
	}

	return allocated_socket;
}

// sets errno on error
int socket_listen(Socket *socket, int backlog,
                  SocketCreateAllocatedFunction create_allocated) {
	socket->create_allocated = create_allocated;

	return socket_listen_platform(socket, backlog);
}

// sets errno on error
int socket_receive(Socket *socket, void *buffer, int length) {
	if (socket->receive == NULL) {
		errno = ENOSYS;

		return -1;
	}

	return socket->receive(socket, buffer, length);
}

// sets errno on error
int socket_send(Socket *socket, const void *buffer, int length) {
	if (socket->send == NULL) {
		errno = ENOSYS;

		return -1;
	}

	return socket->send(socket, buffer, length);
}

// logs errors
int socket_open_server(Socket *socket, const char *address, uint16_t port, bool dual_stack,
                       SocketCreateAllocatedFunction create_allocated) {
	int phase = 0;
	struct addrinfo *resolved_address = NULL;

	log_debug("Opening server socket on port %u", port);

	// resolve listen address
	// FIXME: bind to all returned addresses, instead of just the first one.
	//        requires special handling if IPv4 and IPv6 addresses are returned
	//        and dual-stack mode is enabled
	resolved_address = socket_hostname_to_address(address, port);

	if (resolved_address == NULL) {
		log_error("Could not resolve listen address '%s' (port: %u): %s (%d)",
		          address, port, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	// create socket
	if (socket_create(socket) < 0) {
		log_error("Could not create socket: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	if (socket_open(socket, resolved_address->ai_family,
	                resolved_address->ai_socktype, resolved_address->ai_protocol) < 0) {
		log_error("Could not open %s server socket: %s (%d)",
		          socket_get_address_family_name(resolved_address->ai_family, false),
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	if (resolved_address->ai_family == AF_INET6) {
		if (socket_set_dual_stack(socket, dual_stack) < 0) {
			log_error("Could not %s dual-stack mode for IPv6 server socket: %s (%d)",
			          dual_stack ? "enable" : "disable",
			          get_errno_name(errno), errno);

			goto cleanup;
		}
	}

#ifndef _WIN32
	// on Unix the SO_REUSEADDR socket option allows to rebind sockets in
	// CLOSE-WAIT state. this is a desired effect. on Windows SO_REUSEADDR
	// allows to rebind sockets in any state. this is dangerous. therefore,
	// don't set SO_REUSEADDR on Windows. sockets can be rebound in CLOSE-WAIT
	// state on Windows by default.
	if (socket_set_address_reuse(socket, true) < 0) {
		log_error("Could not enable address-reuse mode for server socket: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}
#endif

	// bind socket and start to listen
	if (socket_bind(socket, resolved_address->ai_addr,
	                resolved_address->ai_addrlen) < 0) {
		log_error("Could not bind %s server socket to '%s' on port %u: %s (%d)",
		          socket_get_address_family_name(resolved_address->ai_family, dual_stack),
		          address, port, get_errno_name(errno), errno);

		goto cleanup;
	}

	if (socket_listen(socket, 10, create_allocated) < 0) {
		log_error("Could not listen to %s server socket bound to '%s' on port %u: %s (%d)",
		          socket_get_address_family_name(resolved_address->ai_family, dual_stack),
		          address, port, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	log_debug("Started listening to '%s' (%s) on port %u",
	          address,
	          socket_get_address_family_name(resolved_address->ai_family, dual_stack),
	          port);

	freeaddrinfo(resolved_address);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		socket_destroy(socket);
		// fall through

	case 1:
		freeaddrinfo(resolved_address);
		// fall through

	default:
		break;
	}

	return phase == 3 ? 0 : -1;
}
