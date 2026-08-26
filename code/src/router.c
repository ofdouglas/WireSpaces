/**
 * @file router.c
 * @brief WireSpaces router implementation.
 */

#include "router.h"

#include <stddef.h>

DispatchResult router_forward_packet(const RouteTable* table, PacketBufferHeader* packet) {
    if (table == NULL || packet == NULL || table->base == NULL) {
        return DISPATCH_NO_ENDPOINT;
    }

    if (table->forward == NULL) {
        return DISPATCH_NO_ENDPOINT;
    }

    for (size_t index = 0U; index < table->capacity; ++index) {
        const RouteTableEntry* entry = &table->base[index];
        if (entry->wire_number != packet->header.wire_number) {
            continue;
        }

        table->forward(table->forwarder_context, packet, entry->egress_set);
        return DISPATCH_OK;
    }

    return DISPATCH_NO_ENDPOINT;
}
