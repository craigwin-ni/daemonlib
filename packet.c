/*
 * daemonlib
 * Copyright (C) 2012-2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * packet.c: Packet definiton for protocol version 2
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
 * functions for validating, packing, unpacking and comparing packets.
 */

#include <stdio.h>

#include "packet.h"

#include "base58.h"
#include "macros.h"
#include "utils.h"

STATIC_ASSERT(sizeof(PacketHeader) == 8, "PacketHeader has invalid size");
STATIC_ASSERT(sizeof(Packet) == 80, "Packet has invalid size");
STATIC_ASSERT(sizeof(EnumerateCallback) == 34, "EnumerateCallback has invalid size");
STATIC_ASSERT(sizeof(GetAuthenticationNonceRequest) == 8, "GetAuthenticationNonceRequest has invalid size");
STATIC_ASSERT(sizeof(GetAuthenticationNonceResponse) == 12, "GetAuthenticationNonceResponse has invalid size");
STATIC_ASSERT(sizeof(AuthenticateRequest) == 32, "AuthenticateRequest has invalid size");
STATIC_ASSERT(sizeof(StackEnumerateRequest) == 8, "StackEnumerateRequest has invalid size");
STATIC_ASSERT(sizeof(StackEnumerateResponse) == 72, "StackEnumerateResponse has invalid size");

int packet_header_is_valid_request(PacketHeader *header, const char **message) {
	if (header->length < (int)sizeof(PacketHeader)) {
		if (message != NULL) {
			*message = "Length is too small";
		}

		return 0;
	}

	if (header->length > (int)sizeof(Packet)) {
		if (message != NULL) {
			*message = "Length is too big";
		}

		return 0;
	}

	if (header->function_id == 0) {
		if (message != NULL) {
			*message = "Invalid function ID";
		}

		return 0;
	}

	if (packet_header_get_sequence_number(header) == 0) {
		if (message != NULL) {
			*message = "Invalid sequence number";
		}

		return 0;
	}

	return 1;
}

int packet_header_is_valid_response(PacketHeader *header, const char **message) {
	if (header->length < (int)sizeof(PacketHeader)) {
		if (message != NULL) {
			*message = "Length is too small";
		}

		return 0;
	}

	if (header->length > (int)sizeof(Packet)) {
		if (message != NULL) {
			*message = "Length is too big";
		}

		return 0;
	}

	if (uint32_from_le(header->uid) == 0) {
		if (message != NULL) {
			*message = "Invalid UID";
		}

		return 0;
	}

	if (header->function_id == 0) {
		if (message != NULL) {
			*message = "Invalid function ID";
		}

		return 0;
	}

	if (!packet_header_get_response_expected(header)) {
		if (message != NULL) {
			*message = "Invalid response expected bit";
		}

		return 0;
	}

	return 1;
}

uint8_t packet_header_get_sequence_number(PacketHeader *header) {
	return (header->sequence_number_and_options >> 4) & 0x0F;
}

void packet_header_set_sequence_number(PacketHeader *header, uint8_t sequence_number) {
	header->sequence_number_and_options |= (sequence_number << 4) & 0xF0;
}

bool packet_header_get_response_expected(PacketHeader *header) {
	return ((header->sequence_number_and_options >> 3) & 0x01) == 0x01;
}

void packet_header_set_response_expected(PacketHeader *header, bool response_expected) {
	header->sequence_number_and_options |= response_expected ? 0x08 : 0x00;
}

PacketE packet_header_get_error_code(PacketHeader *header) {
	return (header->error_code_and_future_use >> 6) & 0x03;
}

void packet_header_set_error_code(PacketHeader *header, PacketE error_code) {
	header->error_code_and_future_use |= (error_code << 6) & 0xC0;
}

const char *packet_get_response_type(Packet *packet) {
	if (packet_header_get_sequence_number(&packet->header) != 0) {
		return "response";
	}

	if (packet->header.function_id != CALLBACK_ENUMERATE) {
		return "callback";
	}

	switch (((EnumerateCallback *)packet)->enumeration_type) {
	case ENUMERATION_TYPE_AVAILABLE:
		return "enumerate-available callback";

	case ENUMERATION_TYPE_CONNECTED:
		return "enumerate-connected callback";

	case ENUMERATION_TYPE_DISCONNECTED:
		return "enumerate-disconnected callback";

	default:
		return "enumerate-<unknown> callback";
	}
}

char *packet_get_request_signature(char *signature, Packet *packet) {
	char base58[BASE58_MAX_LENGTH];

	snprintf(signature, PACKET_MAX_SIGNATURE_LENGTH,
	         "U: %s, L: %u, F: %u, S: %u, R: %u",
	         base58_encode(base58, uint32_from_le(packet->header.uid)),
	         packet->header.length,
	         packet->header.function_id,
	         packet_header_get_sequence_number(&packet->header),
	         packet_header_get_response_expected(&packet->header));

	return signature;
}

char *packet_get_response_signature(char *signature, Packet *packet) {
	char base58[BASE58_MAX_LENGTH];

	if (packet_header_get_sequence_number(&packet->header) != 0) {
		snprintf(signature, PACKET_MAX_SIGNATURE_LENGTH,
		         "U: %s, L: %u, F: %u, S: %u, E: %u",
		         base58_encode(base58, uint32_from_le(packet->header.uid)),
		         packet->header.length,
		         packet->header.function_id,
		         packet_header_get_sequence_number(&packet->header),
		         packet_header_get_error_code(&packet->header));
	} else {
		snprintf(signature, PACKET_MAX_SIGNATURE_LENGTH,
		         "U: %s, L: %u, F: %u",
		         base58_encode(base58, uint32_from_le(packet->header.uid)),
		         packet->header.length,
		         packet->header.function_id);
	}

	return signature;
}

char *packet_get_content_dump(char *content_dump, Packet *packet, int length) {
	int i;

	if (length > (int)sizeof(Packet)) {
		length = (int)sizeof(Packet);
	}

	for (i = 0; i < length; ++i) {
		snprintf(content_dump + i * 3, 4, "%02X ", ((uint8_t *)packet)[i]);
	}

	if (length > 0) {
		content_dump[length * 3 - 1] = '\0';
	} else {
		content_dump[0] = '\0';
	}

	return content_dump;
}

bool packet_is_matching_response(Packet *packet, PacketHeader *pending_request) {
	if (packet->header.uid != pending_request->uid) {
		return false;
	}

	if (packet->header.function_id != pending_request->function_id) {
		return false;
	}

	if (packet_header_get_sequence_number(&packet->header) !=
	    packet_header_get_sequence_number(pending_request)) {
		return false;
	}

	return true;
}
