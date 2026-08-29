/**
 * @file mailbox.h
 * @brief Single-slot Snapshot-style mailbox for Endpoint receive callbacks.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/dispatch.h>
#include <core/ws_constants.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[WS_MAILBOX_DEFAULT_CAPACITY];
    uint16_t length;
    bool occupied;
    uint32_t generation;
} ws_mailbox_t;

void ws_mailbox_init(ws_mailbox_t* mailbox);

bool ws_mailbox_store_from_packet(ws_mailbox_t* mailbox, const ws_packet_buffer_t* packet);

bool ws_mailbox_read(
    const ws_mailbox_t* mailbox,
    uint8_t* out_data,
    size_t out_capacity,
    uint16_t* out_length,
    uint32_t* out_generation);

void ws_mailbox_receive_callback(void* receiver_context, const ws_packet_buffer_t* packet);

#ifdef __cplusplus
}
#endif
