/**
 * @file dispatch.c
 * @brief WireSpaces endpoint dispatch implementation.
 */

#include "dispatch.h"

#include <stddef.h>

#include "host.h"
#include "ws_constants.h"

static bool host_is_member_of_wire(const ws_host_info_t* host_info, uint8_t wire_number) {
    if (host_info == NULL) {
        return false;
    }

    for (uint8_t index = 0U; index < host_info->num_wires; ++index) {
        if (host_info->wires[index] == wire_number) {
            return true;
        }
    }

    return false;
}

static bool packet_is_addressed_to_local_host(const ws_packet_buffer_t* packet) {
    const ws_host_info_t* host_info = ws_host_get_info();
    if (host_info == NULL || packet == NULL) {
        return false;
    }

    if (packet->header.dst_host == host_info->host_id) {
        return true;
    }

    if (packet->header.dst_host == WS_HOST_BROADCAST) {
        return host_is_member_of_wire(host_info, packet->header.wire_number);
    }

    return false;
}

ws_dispatch_result_t ws_dispatch_packet(const ws_dispatch_table_t* table, const ws_packet_buffer_t* packet) {
    if (table == NULL || packet == NULL || table->base == NULL) {
        return WS_DISPATCH_NO_ENDPOINT;
    }

    if (!packet_is_addressed_to_local_host(packet)) {
        return WS_DISPATCH_NO_ENDPOINT;
    }

    for (size_t index = 0U; index < table->capacity; ++index) {
        const ws_dispatch_table_entry_t* entry = &table->base[index];
        if (entry->endpoint != packet->header.endpoint) {
            continue;
        }

        if (entry->receiver.receive == NULL) {
            return WS_DISPATCH_NO_ENDPOINT;
        }

        entry->receiver.receive(entry->receiver.receiver_context, packet);
        return WS_DISPATCH_OK;
    }

    return WS_DISPATCH_NO_ENDPOINT;
}
