/**
 * @file router.c
 * @brief WireSpaces router implementation.
 */

#include "router.h"

#include <stddef.h>

ws_dispatch_result_t ws_router_forward_packet(const ws_route_table_t* table, const ws_packet_buffer_t* packet) {
    if (table == NULL || packet == NULL || table->base == NULL) {
        return WS_DISPATCH_NO_ENDPOINT;
    }

    if (table->forward == NULL) {
        return WS_DISPATCH_NO_ENDPOINT;
    }

    for (size_t index = 0U; index < table->capacity; ++index) {
        const ws_route_table_entry_t* entry = &table->base[index];
        if (entry->wire_number != packet->header.wire_number) {
            continue;
        }

        table->forward(table->forwarder_context, packet, entry->egress_set);
        return WS_DISPATCH_OK;
    }

    return WS_DISPATCH_NO_ENDPOINT;
}
