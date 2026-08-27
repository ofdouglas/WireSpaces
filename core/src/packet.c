/**
 * @file packet.c
 * @brief WireSpaces packet helpers.
 */

#include "packet.h"

#include <stddef.h>

void ws_packet_init(
    ws_packet_buffer_t* packet,
    uint16_t capacity,
    uint16_t size,
    ws_control_fields_t control_fields) {
    if (packet == NULL) {
        return;
    }

    if (size > capacity) {
        size = capacity;
    }

    packet->capacity = capacity;
    packet->size = size;
    ws_header_set_control_fields(&packet->header, control_fields);
}

const uint8_t* ws_packet_payload_bytes(const ws_packet_buffer_t* packet) {
    if (packet == NULL) {
        return NULL;
    }

    return (const uint8_t*)packet + sizeof(ws_packet_buffer_t);
}

uint8_t* ws_packet_payload_mut(ws_packet_buffer_t* packet) {
    if (packet == NULL) {
        return NULL;
    }

    return (uint8_t*)packet + sizeof(ws_packet_buffer_t);
}

void ws_packet_set_endpoint(ws_header_t* header, ws_namespace_t namespace_id, uint16_t endpoint_id) {
    ws_header_set_endpoint(header, namespace_id, endpoint_id);
}
