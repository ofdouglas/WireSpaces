/**
 * @file mailbox.c
 * @brief Single-slot mailbox implementation.
 */

#include "mailbox.h"

#include <stddef.h>
#include <string.h>

#include "packet.h"

void mailbox_init(Mailbox* mailbox) {
    if (mailbox == NULL) {
        return;
    }

    mailbox->length = 0U;
    mailbox->generation = 0U;
    mailbox->occupied = false;
    memset(mailbox->data, 0, sizeof(mailbox->data));
}

bool mailbox_store_from_packet(Mailbox* mailbox, PacketBufferHeader* packet) {
    if (mailbox == NULL || packet == NULL) {
        return false;
    }

    const uint8_t* payload = packet_payload_bytes(packet);
    if (payload == NULL) {
        return false;
    }

    if (packet->length > WS_MAILBOX_DEFAULT_CAPACITY) {
        return false;
    }

    if (packet->length > 0U) {
        memcpy(mailbox->data, payload, packet->length);
    }

    mailbox->length = packet->length;
    mailbox->occupied = true;
    mailbox->generation += 1U;

    return true;
}

bool mailbox_read(
    const Mailbox* mailbox,
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

void mailbox_receive_callback(void* receiver_context, PacketBufferHeader* packet) {
    if (receiver_context == NULL) {
        return;
    }

    mailbox_store_from_packet((Mailbox*)receiver_context, packet);
}
