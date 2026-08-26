/**
 * @file packet_builder.hpp
 * @brief Builds in-memory test packets with sane local-domain defaults.
 */

#pragma once

#include <cstring>

#include "header.h"
#include "packet.h"
#include "ws_constants.h"

#include "support/constants.hpp"

namespace wirespaces::test::support {

WS_PACKET_DEFINE(TestPacket, WS_MAILBOX_DEFAULT_CAPACITY);

inline ControlFields defaultControlFields() {
    return ControlFields{QOS_NORMAL, false, TRANSPORT_SIMPLE};
}

inline PacketBufferHeader* asPacketBuffer(TestPacket* packet) {
    return reinterpret_cast<PacketBufferHeader*>(packet);
}

inline const PacketBufferHeader* asPacketBuffer(const TestPacket* packet) {
    return reinterpret_cast<const PacketBufferHeader*>(packet);
}

class PacketBuilder {
public:
    PacketBuilder() {
        packet_ = TestPacket{};
        packet_init(asPacketBuffer(&packet_), 0U, defaultControlFields());
        packet_.header.wire_number = WS_WIRE_LOCAL_DOMAIN;
        packet_.header.src_host = kLocalHostId;
        packet_.header.dst_host = kLocalHostId;
    }

    PacketBuilder& withLength(uint16_t length) {
        asPacketBuffer(&packet_)->length = length;
        return *this;
    }

    PacketBuilder& withPayload(const char* text) {
        const size_t length = std::strlen(text);
        withLength(static_cast<uint16_t>(length));
        if (length > 0U) {
            std::memcpy(packet_.data, text, length);
        }
        return *this;
    }

    PacketBuilder& withPayloadLength(uint16_t length) {
        withLength(length);
        for (uint16_t index = 0U; index < length; ++index) {
            packet_.data[index] = static_cast<uint8_t>('A' + (index % 26U));
        }
        return *this;
    }

    PacketBuilder& withWire(uint8_t wire_number) {
        packet_.header.wire_number = wire_number;
        return *this;
    }

    PacketBuilder& withSrcHost(uint8_t host_id) {
        packet_.header.src_host = host_id;
        return *this;
    }

    PacketBuilder& withDstHost(uint8_t host_id) {
        packet_.header.dst_host = host_id;
        return *this;
    }

    PacketBuilder& withEndpoint(WsNamespace namespace_id, uint16_t endpoint_id) {
        packet_set_endpoint(&packet_.header, namespace_id, endpoint_id);
        return *this;
    }

    PacketBuilder& withRawEndpoint(uint16_t endpoint) {
        packet_.header.endpoint = endpoint;
        return *this;
    }

    PacketBuilder& withControlFields(const ControlFields& control_fields) {
        header_set_control_fields(&packet_.header, control_fields);
        return *this;
    }

    TestPacket& packet() {
        return packet_;
    }

    const TestPacket& packet() const {
        return packet_;
    }

    PacketBufferHeader* packetBuffer() {
        return asPacketBuffer(&packet_);
    }

private:
    TestPacket packet_{};
};

} // namespace wirespaces::test::support
