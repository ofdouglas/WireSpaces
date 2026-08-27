/**
 * @file packet.h
 * @brief WireSpaces packet definition and accessors API.
 */

#pragma once

#include "header.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t capacity;
    uint16_t size;
    ws_header_t header;
} WS_PACKED ws_packet_buffer_t;

#define WS_PACKET_DEFINE(name, payload_capacity) \
    typedef struct { \
        uint16_t capacity; \
        uint16_t size; \
        ws_header_t header; \
        uint8_t data[(payload_capacity)]; \
    } WS_PACKED name

#if defined(__cplusplus)
static_assert(
    offsetof(ws_packet_buffer_t, header) + sizeof(ws_header_t) == sizeof(ws_packet_buffer_t),
    "ws_packet_buffer_t must not contain padding before payload");
#else
_Static_assert(
    offsetof(ws_packet_buffer_t, header) + sizeof(ws_header_t) == sizeof(ws_packet_buffer_t),
    "ws_packet_buffer_t must not contain padding before payload");
#endif

void ws_packet_init(
    ws_packet_buffer_t* packet,
    uint16_t capacity,
    uint16_t size,
    ws_control_fields_t control_fields);

const uint8_t* ws_packet_payload_bytes(const ws_packet_buffer_t* packet);
uint8_t* ws_packet_payload_mut(ws_packet_buffer_t* packet);

void ws_packet_set_endpoint(ws_header_t* header, ws_namespace_t namespace_id, uint16_t endpoint_id);

#ifdef __cplusplus
}
#endif
