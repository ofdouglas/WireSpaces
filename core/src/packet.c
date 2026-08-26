/**
 * @file packet.c
 * @brief WireSpaces packet helpers.
 */

#include "packet.h"

#include <stddef.h>

void packet_init(PacketBufferHeader* packet, uint16_t length, ControlFields control_fields) {
    if (packet == NULL) {
        return;
    }

    packet->length = length;
    header_set_control_fields(&packet->header, control_fields);
}

uint8_t* packet_payload_bytes(PacketBufferHeader* packet) {
    if (packet == NULL) {
        return NULL;
    }

    return (uint8_t*)packet + sizeof(PacketBufferHeader);
}

void packet_set_endpoint(Header* header, WsNamespace namespace_id, uint16_t endpoint_id) {
    header_set_endpoint(header, namespace_id, endpoint_id);
}
