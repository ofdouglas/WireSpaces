/**
 * @file stub_hello_receiver.hpp
 * @brief Stub receiver service using a 1-slot mailbox.
 */

#pragma once

#include <cstdint>
#include <string>

#include <runtime/core.hpp>

namespace wirespaces::test {

constexpr uint16_t kHelloSenderEndpoint = 0x0001U;
constexpr uint16_t kHelloReceiverEndpoint = 0x0002U;

class HelloReceiver {
public:
    HelloReceiver() {
        ws_mailbox_init(&mailbox_);
    }

    void receive(const PacketBuffer* packet) {
        ws_mailbox_receive_callback(&mailbox_, packet);
    }

    [[nodiscard]] EndpointReceiver receiverHandle() {
        return EndpointReceiver{endpoint_receive_thunk<HelloReceiver>, this};
    }

    [[nodiscard]] uint16_t endpointId() const {
        return kHelloReceiverEndpoint;
    }

    [[nodiscard]] bool hasMessage() const {
        return mailbox_.occupied;
    }

    [[nodiscard]] uint32_t generation() const {
        return mailbox_.generation;
    }

    [[nodiscard]] std::string text() const {
        uint8_t buffer[WS_MAILBOX_DEFAULT_CAPACITY]{};
        uint16_t length = 0U;
        uint32_t generation = 0U;
        if (!ws_mailbox_read(&mailbox_, buffer, sizeof(buffer), &length, &generation)) {
            return {};
        }

        return std::string(reinterpret_cast<char*>(buffer), length);
    }

    [[nodiscard]] const Mailbox& mailbox() const {
        return mailbox_;
    }

private:
    Mailbox mailbox_{};
};

} // namespace wirespaces::test
