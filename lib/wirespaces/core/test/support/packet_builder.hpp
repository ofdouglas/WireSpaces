/**
 * @file packet_builder.hpp
 * @brief Builds fixed-storage test packets with local-domain defaults.
 */

#pragma once

#include <cstring>

#include <wirespaces/runtime/core.hpp>

#include "support/constants.hpp"

namespace wirespaces::test::support {

WS_PACKET_BUFFER_DEFINE(TestPacket, kDefaultEndpointStorageCapacity);

inline ControlFields defaultControlFields() {
    return ControlFields{QoS::kNormal, false, TransportType::kSimple};
}

inline PacketBuffer* asPacketBuffer(TestPacket* packet) { return packet; }
inline const PacketBuffer* asPacketBuffer(const TestPacket* packet) { return packet; }

class PacketBuilder {
public:
    PacketBuilder() {
        static_cast<void>(packet_.initialize(0U, defaultControlFields()));
        packet_.header().wire = kLocalWire;
        packet_.header().source = kLocalHostId;
        packet_.header().destination = kLocalHostId;
    }

    PacketBuilder& withSize(uint16_t size) {
        static_cast<void>(packet_.resize(size));
        return *this;
    }

    PacketBuilder& withPayload(const char* text) {
        const size_t length{std::strlen(text)};
        withSize(static_cast<uint16_t>(length));
        if (length > 0U) {
            std::memcpy(packet_.payload().data(), text, length);
        }
        return *this;
    }

    PacketBuilder& withPayloadLength(uint16_t length) {
        withSize(length);
        MutableByteSpan payload{packet_.payload()};
        for (uint16_t index = 0U; index < length; ++index) {
            payload[index] = static_cast<uint8_t>('A' + (index % 26U));
        }
        return *this;
    }

    PacketBuilder& withWire(WireNumber wire) {
        packet_.header().wire = wire;
        return *this;
    }

    PacketBuilder& withSource(HostId host) {
        packet_.header().source = host;
        return *this;
    }

    PacketBuilder& withDestination(HostId host) {
        packet_.header().destination = host;
        return *this;
    }

    PacketBuilder& withEndpoint(EndpointAddress endpoint) {
        packet_.header().endpoint = endpoint;
        return *this;
    }

    PacketBuilder& withControlFields(ControlFields control_fields) {
        packet_.header().setControlFields(control_fields);
        return *this;
    }

    TestPacket& packet() { return packet_; }
    const TestPacket& packet() const { return packet_; }
    PacketBuffer& packetBuffer() { return packet_; }

private:
    TestPacket packet_{};
};

} // namespace wirespaces::test::support
