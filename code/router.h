/**
 * @file router.h
 * @brief WireSpaces Router: forwards packets to the appropriate links. All packets are also
 *        handed to the local endpoint domain; it filters out packets not addressed to this host.
 */
#pragma once

#include "dispatch.h"
#ifdef __cplusplus
namespace wirespaces {
#endif

// EgressSet defines a set of links to forward to. It can be a bitmask or an enum.
#define WS_EGRESS_SET_NONE         0

typedef uint8_t EgressSet;

typedef struct {
    uint8_t wire_number;
    EgressSet egress_set;
} RouteTableEntry;

typedef struct {
    RouteTableEntry* base;
    size_t capacity;

    // User-provided function and member data, must be re-entrant. See example code below.
    void (*forward)(void* forwarder_context, PacketBufferHeader* packet, uint8_t egress_set);
    void *forwarder_context;
} RouteTable;

// Forward the packet to the local domain and all links specified in the egress set.
DispatchResult router_forward_packet(const RouteTable* table, PacketBufferHeader* packet);


// Example user implementation
void router_forward_packet_impl(void* forwarder_context, PacketBufferHeader* packet, uint8_t egress_set) {
    const RouterForwardingCtx* ctx = (RouterForwardingCtx*)(forwarder_context);

    dispatch_packet(ctx->dispatch_table, packet);

    if (egress_set & WS_EGRESS_SET_CAN_A /* hypothetical example */) {
        wscan_send(ctx->can_a_, packet);
    }

    // ... etc
}

#ifdef __cplusplus
} // namespace wirespaces
#endif
