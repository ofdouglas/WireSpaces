/**
 * @file local_domain.c
 * @brief Local-domain forwarding glue.
 */

#include "local_domain.h"

#include <stddef.h>

void local_domain_forward_impl(
    void* forwarder_context,
    PacketBufferHeader* packet,
    uint8_t egress_set) {
    (void)egress_set;

    if (forwarder_context == NULL) {
        return;
    }

    const LocalDomainForwardContext* context = (const LocalDomainForwardContext*)forwarder_context;
    if (context->dispatch_table == NULL) {
        return;
    }

    dispatch_packet(context->dispatch_table, packet);
}

DispatchResult local_domain_forward(RouteTable* route_table, PacketBufferHeader* packet) {
    if (route_table == NULL || packet == NULL) {
        return DISPATCH_NO_ENDPOINT;
    }

    return router_forward_packet(route_table, packet);
}
