/**
 * @file mailbox.h
 * @brief Single-slot Snapshot-style mailbox for Endpoint receive callbacks.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dispatch.h"
#include "ws_constants.h"

#ifdef __cplusplus
namespace wirespaces {
extern "C" {
#endif

typedef struct {
    uint8_t data[WS_MAILBOX_DEFAULT_CAPACITY];
    uint16_t length;
    bool occupied;
    uint32_t generation;
} Mailbox;

void mailbox_init(Mailbox* mailbox);

bool mailbox_store_from_packet(Mailbox* mailbox, PacketBufferHeader* packet);

bool mailbox_read(
    const Mailbox* mailbox,
    uint8_t* out_data,
    size_t out_capacity,
    uint16_t* out_length,
    uint32_t* out_generation);

void mailbox_receive_callback(void* receiver_context, PacketBufferHeader* packet);

#ifdef __cplusplus
} // extern "C"
} // namespace wirespaces
#endif
