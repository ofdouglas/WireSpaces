/**
 * @file dispatch.h
 * @brief WireSpaces Endpoint Dispatcher -- finds the right endpoint for a packet and dispatches it.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "header.h"
#include "packet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WS_DISPATCH_OK = 0,
    WS_DISPATCH_NO_ENDPOINT,
} ws_dispatch_result_t;

typedef void (*ws_receive_callback_t)(void* receiver_context, const ws_packet_buffer_t* packet);

typedef struct {
    ws_receive_callback_t receive;
    void* receiver_context;
} ws_endpoint_receiver_t;

typedef struct {
    uint16_t endpoint;
    ws_endpoint_receiver_t receiver;
} ws_dispatch_table_entry_t;

typedef struct {
    ws_dispatch_table_entry_t* base;
    size_t capacity;
} ws_dispatch_table_t;

/** Deliver the packet to the appropriate endpoint when addressed to this host. */
ws_dispatch_result_t ws_dispatch_packet(const ws_dispatch_table_t* table, const ws_packet_buffer_t* packet);

#ifdef __cplusplus
}
#endif
