/**
 * @file local_domain.c
 * @brief Local-domain forwarding glue.
 */

#include <core/local_domain.h>

#include <stddef.h>

void ws_local_domain_forward_impl(
    void* forwarder_context,
    const ws_packet_buffer_t* packet,
    ws_egress_set_t egress_set) {
    (void)egress_set;

    if (forwarder_context == NULL) {
        return;
    }

    const ws_local_domain_forward_context_t* context =
        (const ws_local_domain_forward_context_t*)forwarder_context;
    if (context->dispatch_table == NULL) {
        return;
    }

    ws_dispatch_packet(context->dispatch_table, packet);
}

ws_dispatch_result_t ws_local_domain_forward(ws_route_table_t* route_table, const ws_packet_buffer_t* packet) {
    if (route_table == NULL || packet == NULL) {
        return WS_DISPATCH_NO_ENDPOINT;
    }

    return ws_router_forward_packet(route_table, packet);
}
