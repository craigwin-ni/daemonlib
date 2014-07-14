/*
 * daemonlib
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * writer.c: Buffered packet writer for I/O devices
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
#include <string.h>

#include "writer.h"

#include "event.h"
#include "log.h"

#define LOG_CATEGORY LOG_CATEGORY_OTHER

#define MAX_QUEUED_WRITES 32768

static void writer_handle_write(void *opaque) {
	Writer *writer = opaque;
	Packet *packet;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];
	char recipient_signature[WRITER_MAX_RECIPIENT_SIGNATURE_LENGTH];

	if (writer->backlog.count == 0) {
		return;
	}

	packet = queue_peek(&writer->backlog);

	if (io_write(writer->io, packet, packet->header.length) < 0) {
		log_error("Could not send queued %s (%s) to %s, disconnecting %s: %s (%d)",
		          writer->packet_type,
		          writer->packet_signature(packet_signature, packet),
		          writer->recipient_signature(recipient_signature, 0, writer->opaque),
		          writer->recipient_name,
		          get_errno_name(errno), errno);

		writer->recipient_disconnect(writer->opaque);

		return;
	}

	queue_pop(&writer->backlog, NULL);

	log_debug("Sent queued %s (%s) to %s, %d %s(s) left in write backlog",
	          writer->packet_type,
	          writer->packet_signature(packet_signature, packet),
	          writer->recipient_signature(recipient_signature, 0, writer->opaque),
	          writer->backlog.count,
	          writer->packet_type);

	if (writer->backlog.count == 0) {
		// last queued response handled, deregister for write events
		event_modify_source(writer->io->handle, EVENT_SOURCE_TYPE_GENERIC,
		                    EVENT_WRITE, 0, NULL, NULL);
	}
}

static int writer_push_packet_to_backlog(Writer *writer, Packet *packet) {
	Packet *queued_packet;
	char recipient_signature[WRITER_MAX_RECIPIENT_SIGNATURE_LENGTH];
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];
	uint32_t packets_to_drop;

	log_debug("%s is not ready to receive, pushing %s to write backlog (count: %d +1)",
	          writer->recipient_signature(recipient_signature, 1, writer->opaque),
	          writer->packet_type, writer->backlog.count);

	if (writer->backlog.count >= MAX_QUEUED_WRITES) {
		packets_to_drop = writer->backlog.count - MAX_QUEUED_WRITES + 1;

		log_warn("Write backlog for %s is full, dropping %u queued %s(s), %u +%u dropped in total",
		         writer->recipient_signature(recipient_signature, 0, writer->opaque),
		         packets_to_drop, writer->packet_type,
		         writer->dropped_packets, packets_to_drop);

		writer->dropped_packets += packets_to_drop;

		while (writer->backlog.count >= MAX_QUEUED_WRITES) {
			queue_pop(&writer->backlog, NULL);
		}
	}

	queued_packet = queue_push(&writer->backlog);

	if (queued_packet == NULL) {
		log_error("Could not push %s (%s) to write backlog for %s, discarding %s: %s (%d)",
		          writer->packet_type,
		          writer->packet_signature(packet_signature, packet),
		          writer->recipient_signature(recipient_signature, 0, writer->opaque),
		          writer->packet_type,
		          get_errno_name(errno), errno);

		return -1;
	}

	memcpy(queued_packet, packet, packet->header.length);

	if (writer->backlog.count == 1) {
		// first queued packet, register for write events
		if (event_modify_source(writer->io->handle, EVENT_SOURCE_TYPE_GENERIC,
		                        0, EVENT_WRITE, writer_handle_write, writer) < 0) {
			// FIXME: how to handle this error?
			return -1;
		}
	}

	return 0;
}

int writer_create(Writer *writer, IO *io,
                  const char *packet_type,
                  WriterPacketSignatureFunction packet_signature,
                  const char *recipient_name,
                  WriterRecipientSignatureFunction recipient_signature,
                  WriterRecipientDisconnectFunction recipient_disconnect,
                  void *opaque) {
	writer->io = io;
	writer->packet_type = packet_type;
	writer->packet_signature = packet_signature;
	writer->recipient_name = recipient_name;
	writer->recipient_signature = recipient_signature;
	writer->recipient_disconnect = recipient_disconnect;
	writer->opaque = opaque;
	writer->dropped_packets = 0;

	// create write queue
	if (queue_create(&writer->backlog, sizeof(Packet)) < 0) {
		log_error("Could not create backlog: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	return 0;
}

void writer_destroy(Writer *writer) {
	char recipient_signature[WRITER_MAX_RECIPIENT_SIGNATURE_LENGTH];

	if (writer->backlog.count > 0) {
		log_warn("Destroying writer for %s while %d %s(s) have not been send",
		         writer->recipient_signature(recipient_signature, 0, writer->opaque),
		         writer->backlog.count,
		         writer->packet_type);

		event_modify_source(writer->io->handle, EVENT_SOURCE_TYPE_GENERIC,
		                    EVENT_WRITE, 0, NULL, NULL);
	}

	queue_destroy(&writer->backlog, NULL);
}

// returns -1 on error, 0 if the packet was written and 1 if the packet was enqueued
int writer_write(Writer *writer, Packet *packet) {
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];
	char recipient_signature[WRITER_MAX_RECIPIENT_SIGNATURE_LENGTH];

	if (writer->backlog.count == 0) {
		// if there is no backlog, try to write
		if (io_write(writer->io, packet, packet->header.length) < 0) {
			// if write fails with an error different from EWOULDBLOCK then give up
			// and disconnect recipient
			if (!errno_would_block()) {
				log_error("Could not send %s (%s) to %s, disconnecting %s: %s (%d)",
				          writer->packet_type,
				          writer->packet_signature(packet_signature, packet),
				          writer->recipient_signature(recipient_signature, 0, writer->opaque),
				          writer->recipient_name,
				          get_errno_name(errno), errno);

				writer->recipient_disconnect(writer->opaque);

				return -1;
			}
		} else {
			// successfully written
			return 0;
		}
	}

	// if either there is already a backlog or a write failed with EWOULDBLOCK
	// then push to backlog
	if (writer_push_packet_to_backlog(writer, packet) < 0) {
		return -1;
	}

	// successfully pushed to backlog
	return 1;
}
