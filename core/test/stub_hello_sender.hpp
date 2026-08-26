/**
 * @file stub_hello_sender.hpp
 * @brief Stub sender service for local-domain hello packets.
 */

#pragma once

#include <cstring>
#include <cstdint>

#include "header.h"
#include "local_domain.h"
#include "packet.h"
#include "router.h"
#include "ws_constants.h"

namespace wirespaces {
namespace test {

WS_PACKET_DEFINE(HelloPacket, WS_MAILBOX_DEFAULT_CAPACITY);

class HelloSender {
public:
    HelloSender(RouteTable* route_table, uint16_t receiver_endpoint, uint8_t host_id)
        : route_table_{route_table}
        , receiver_endpoint_{receiver_endpoint}
        , host_id_{host_id} {}

    void sendMessage(const char* message) {
        if (route_table_ == nullptr || message == nullptr) {
            return;
        }

        HelloPacket packet{};
        const uint16_t message_length = static_cast<uint16_t>(std::strlen(message));
        const ControlFields control_fields{QOS_NORMAL, false, TRANSPORT_SIMPLE};
        auto* packet_header = reinterpret_cast<PacketBufferHeader*>(&packet);
        packet_init(packet_header, message_length, control_fields);

        packet.header.wire_number = WS_WIRE_LOCAL_DOMAIN;
        packet.header.src_host = host_id_;
        packet.header.dst_host = host_id_;
        packet_set_endpoint(&packet.header, WS_NAMESPACE_USER0, receiver_endpoint_);

        if (message_length > 0U) {
            std::memcpy(packet.data, message, message_length);
        }

        local_domain_forward(route_table_, packet_header);
    }

    void sendHello() {
        sendMessage("hello");
    }

    [[nodiscard]] uint16_t endpointId() const {
        return kHelloSenderEndpoint;
    }

private:
    RouteTable* route_table_{nullptr};
    uint16_t receiver_endpoint_{0U};
    uint8_t host_id_{0U};
};

} // namespace test
} // namespace wirespaces
