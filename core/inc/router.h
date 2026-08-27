/**
 * @file router.h
 * @brief WireSpaces Router: forwards packets to the appropriate links.
 */

#pragma once

#include "dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WS_EGRESS_SET_NONE 0U

typedef uint8_t ws_egress_set_t;

typedef struct {
    uint8_t wire_number;
    ws_egress_set_t egress_set;
} ws_route_table_entry_t;

typedef struct {
    ws_route_table_entry_t* base;
    size_t capacity;

    void (*forward)(
        void* forwarder_context,
        const ws_packet_buffer_t* packet,
        ws_egress_set_t egress_set);
    void* forwarder_context;
} ws_route_table_t;

ws_dispatch_result_t ws_router_forward_packet(const ws_route_table_t* table, const ws_packet_buffer_t* packet);

#ifdef __cplusplus
}
#endif
