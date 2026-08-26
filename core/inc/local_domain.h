/**
 * @file local_domain.h
 * @brief Convenience helpers for local-domain forwarding.
 */

#pragma once

#include "dispatch.h"
#include "router.h"

#ifdef __cplusplus
namespace wirespaces {
extern "C" {
#endif

typedef struct {
    DispatchTable* dispatch_table;
} LocalDomainForwardContext;

void local_domain_forward_impl(
    void* forwarder_context,
    PacketBufferHeader* packet,
    uint8_t egress_set);

DispatchResult local_domain_forward(RouteTable* route_table, PacketBufferHeader* packet);

#ifdef __cplusplus
} // extern "C"
} // namespace wirespaces
#endif
