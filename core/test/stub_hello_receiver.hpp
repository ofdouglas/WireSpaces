/**
 * @file stub_hello_receiver.hpp
 * @brief Stub EndpointReceiver backed by a snapshot receiver.
 */

#pragma once

#include <string>

#include <runtime/core.hpp>

namespace wirespaces::test {

constexpr uint16_t kHelloSenderEndpointId{0x0001U};
constexpr uint16_t kHelloReceiverEndpointId{0x0002U};
constexpr EndpointAddress kHelloReceiverEndpoint{
    EndpointAddress::from(Namespace::kUser0, kHelloReceiverEndpointId)};

class HelloReceiver final : public EndpointReceiver {
public:
    ReceiveResult receive(const PacketBuffer& packet) noexcept override {
        return snapshot_.receive(packet);
    }

    [[nodiscard]] bool hasMessage() const noexcept { return snapshot_.hasValue(); }
    [[nodiscard]] uint32_t generation() const noexcept { return snapshot_.generation(); }

    [[nodiscard]] std::string text() const {
        uint8_t buffer[kDefaultEndpointStorageCapacity]{};
        uint16_t length{0U};
        uint32_t generation_value{0U};
        if (!snapshot_.read(MutableByteSpan{buffer}, length, generation_value)) {
            return {};
        }
        return std::string{reinterpret_cast<char*>(buffer), length};
    }

private:
    EndpointSnapshotReceiver snapshot_{};
};

} // namespace wirespaces::test
