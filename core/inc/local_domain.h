/**
 * @file local_domain.h
 * @brief Convenience helpers for local-domain forwarding.
 */

#pragma once

#include "dispatch.h"
#include "router.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ws_dispatch_table_t* dispatch_table;
} ws_local_domain_forward_context_t;

void ws_local_domain_forward_impl(
    void* forwarder_context,
    const ws_packet_buffer_t* packet,
    ws_egress_set_t egress_set);

ws_dispatch_result_t ws_local_domain_forward(ws_route_table_t* route_table, const ws_packet_buffer_t* packet);

#ifdef __cplusplus
}
#endif
