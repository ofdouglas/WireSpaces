/**
 * @file stub_hello_sender.hpp
 * @brief Stub sender for local-domain hello packets.
 */

#pragma once

#include <cstring>

#include <wirespaces/runtime/core.hpp>

namespace wirespaces::test {

WS_PACKET_BUFFER_DEFINE(HelloPacketBuffer, kDefaultEndpointStorageCapacity);

class HelloSender {
public:
    HelloSender(Router& router, EndpointAddress receiver_endpoint, HostId host_id) noexcept
        : router_{router}
        , receiver_endpoint_{receiver_endpoint}
        , host_id_{host_id} {}

    void sendMessage(const char* message) {
        if (message == nullptr) {
            return;
        }

        const size_t message_length{std::strlen(message)};
        if (message_length > kDefaultEndpointStorageCapacity) {
            return;
        }

        HelloPacketBuffer packet{};
        if (!packet.initialize(
                static_cast<uint16_t>(message_length),
                ControlFields{QoS::kNormal, false, TransportType::kSimple})) {
            return;
        }

        packet.header().wire = WireNumber{kLocalWireValue};
        packet.header().source = host_id_;
        packet.header().destination = host_id_;
        packet.header().endpoint = receiver_endpoint_;
        if (message_length > 0U) {
            std::memcpy(packet.payload().data(), message, message_length);
        }
        static_cast<void>(router_.forward(packet));
    }

    void sendHello() { sendMessage("hello"); }

private:
    Router& router_;
    EndpointAddress receiver_endpoint_{};
    HostId host_id_{};
};

} // namespace wirespaces::test
