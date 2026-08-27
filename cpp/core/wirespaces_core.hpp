#pragma once
/*
 * @file  wirespaces_core.hpp
 * @brief C++17 wrapper over the WireSpaces C core API.
 */

#include "dispatch.h"
#include "header.h"
#include "host.h"
#include "local_domain.h"
#include "mailbox.h"
#include "packet.h"
#include "router.h"
#include "ws_constants.h"

namespace wirespaces {

using QoS = ws_qos_t;
using TransportType = ws_transport_type_t;
using Namespace = ws_namespace_t;
using Header = ws_header_t;
using ControlFields = ws_control_fields_t;
using PacketBuffer = ws_packet_buffer_t;
using DispatchResult = ws_dispatch_result_t;
using ReceiveCallback = ws_receive_callback_t;
using EndpointReceiver = ws_endpoint_receiver_t;
using DispatchTableEntry = ws_dispatch_table_entry_t;
using DispatchTable = ws_dispatch_table_t;
using Mailbox = ws_mailbox_t;
using EgressSet = ws_egress_set_t;
using RouteTableEntry = ws_route_table_entry_t;
using RouteTable = ws_route_table_t;
using HostInfo = ws_host_info_t;
using LocalDomainForwardContext = ws_local_domain_forward_context_t;

constexpr ws_qos_t kQoSCritical = WS_QOS_CRITICAL;
constexpr ws_qos_t kQoSHigh = WS_QOS_HIGH;
constexpr ws_qos_t kQoSNormal = WS_QOS_NORMAL;
constexpr ws_qos_t kQoSBackground = WS_QOS_BACKGROUND;

constexpr ws_transport_type_t kTransportSimple = WS_TRANSPORT_SIMPLE;

constexpr ws_namespace_t kNamespaceUser0 = WS_NAMESPACE_USER0;
constexpr ws_namespace_t kNamespaceUser1 = WS_NAMESPACE_USER1;
constexpr ws_namespace_t kNamespaceUser2 = WS_NAMESPACE_USER2;
constexpr ws_namespace_t kNamespaceCommon = WS_NAMESPACE_COMMON;

constexpr ws_dispatch_result_t kDispatchOk = WS_DISPATCH_OK;
constexpr ws_dispatch_result_t kDispatchNoEndpoint = WS_DISPATCH_NO_ENDPOINT;

/**
 * @brief Adapts a C++ endpoint receiver to ws_receive_callback_t.
 *
 * @tparam T Endpoint type with void receive(const ws_packet_buffer_t* packet).
 */
template <typename T>
void endpoint_receive_thunk(void* context, const ws_packet_buffer_t* packet) {
    static_cast<T*>(context)->receive(packet);
}

} // namespace wirespaces
