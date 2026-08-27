/**
 * @file mailbox.c
 * @brief Single-slot mailbox implementation.
 */

#include "mailbox.h"

#include <stddef.h>
#include <string.h>

#include "packet.h"

void ws_mailbox_init(ws_mailbox_t* mailbox) {
    if (mailbox == NULL) {
        return;
    }

    mailbox->length = 0U;
    mailbox->generation = 0U;
    mailbox->occupied = false;
    memset(mailbox->data, 0, sizeof(mailbox->data));
}

bool ws_mailbox_store_from_packet(ws_mailbox_t* mailbox, const ws_packet_buffer_t* packet) {
    if (mailbox == NULL || packet == NULL) {
        return false;
    }

    const uint8_t* payload = ws_packet_payload_bytes(packet);
    if (payload == NULL) {
        return false;
    }

    if (packet->size > packet->capacity) {
        return false;
    }

    if (packet->size > sizeof(mailbox->data)) {
        return false;
    }

    if (packet->size > 0U) {
        memcpy(mailbox->data, payload, packet->size);
    }

    mailbox->length = packet->size;
    mailbox->occupied = true;
    mailbox->generation += 1U;

    return true;
}

bool ws_mailbox_read(
    const ws_mailbox_t* mailbox,
    uint8_t* out_data,
    size_t out_capacity,
    uint16_t* out_length,
    uint32_t* out_generation) {
    if (mailbox == NULL || out_data == NULL || out_length == NULL || out_generation == NULL) {
        return false;
    }

    if (!mailbox->occupied) {
        return false;
    }

    if (mailbox->length > out_capacity) {
        return false;
    }

    if (mailbox->length > 0U) {
        memcpy(out_data, mailbox->data, mailbox->length);
    }

    *out_length = mailbox->length;
    *out_generation = mailbox->generation;
    return true;
}

void ws_mailbox_receive_callback(void* receiver_context, const ws_packet_buffer_t* packet) {
    if (receiver_context == NULL) {
        return;
    }

    ws_mailbox_store_from_packet((ws_mailbox_t*)receiver_context, packet);
}
