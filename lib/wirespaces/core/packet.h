/**
 * @file packet.h
 * @brief Fixed-storage, non-templated WireSpaces packet buffer API.
 */

#pragma once

#include <wirespaces/core/header.h>
#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces {

using ByteSpan = foundation::Span<const uint8_t>;
using MutableByteSpan = foundation::Span<uint8_t>;

/**
 * @brief Common packet prefix used by all statically sized packet buffers.
 *
 * Payload storage immediately follows this object and is supplied by
 * WS_PACKET_BUFFER_DEFINE. Packet-processing code remains non-templated.
 */
class alignas(4) PacketBuffer {
public:
    // Payload capacity.
    uint16_t capacity() const noexcept {
        return capacity_;
    }
    // Size of the payload.
    uint16_t size() const noexcept {
        return size_;
    }
    // Size of the packet including the header.
    uint16_t totalSize() const noexcept {
        return size_ + sizeof(Header);
    }

    // Host-local metadata, never serialized: zero means local origin; 1..8
    // identify the receiving interface by its egress bit + 1.
    uint8_t ingressIndex() const noexcept { return ingress_index_; }
    void setIngressIndex(uint8_t index) noexcept { ingress_index_ = index; }

    [[nodiscard]] bool resize(uint16_t size) noexcept;
    /**
     * @brief Copy packet contents into this buffer without changing its capacity.
     *
     * Copies the header, ingress tag, active payload size and active payload bytes. Self-copy
     * succeeds. Insufficient capacity leaves this buffer unchanged. Source and
     * destination must be distinct nonoverlapping buffers unless they are identical.
     */
    [[nodiscard]] bool copyFrom(const PacketBuffer& source) noexcept;
    // Successful initialization (including responses) clears ingress; resize preserves it.
    [[nodiscard]] bool initialize(uint16_t size, ControlFields control_fields) noexcept;
    /**
     * @brief Initialize local-origin size, controls and every address field.
     *
     * Success clears ingress and leaves payload bytes and capacity unchanged.
     * Insufficient capacity leaves the entire packet unchanged. Address validity
     * remains the responsibility of configuration and the routing/service boundary.
     */
    [[nodiscard]] bool initialize(uint16_t size, const ConnectionAddress& connection,
                                  ControlFields control_fields) noexcept;
    [[nodiscard]] bool initializeResponseTo(const Header& request_header, uint16_t size, ControlFields control_fields) noexcept;

    Header& header() noexcept {
        return header_;
    }
    const Header& header() const noexcept {
        return header_;
    }

    MutableByteSpan payload() noexcept;
    ByteSpan payload() const noexcept;
    ByteSpan headerAndPayload() const noexcept;

protected:
    explicit constexpr PacketBuffer(uint16_t capacity) noexcept : capacity_{capacity} {}

private:
    uint16_t capacity_{0U};
    uint16_t size_{0U};
    uint8_t ingress_index_{0U};
    uint8_t payload_alignment_padding_{0U};
    Header header_{};
};

// Keep this small configuration adapter visible so constant addresses can be folded on MCUs.
inline bool PacketBuffer::initialize(uint16_t size, const ConnectionAddress& connection,
                                     ControlFields control_fields) noexcept {
    if (!initialize(size, control_fields)) {
        return false;
    }
    header_.wire = connection.wire;
    header_.source = connection.local_host;
    header_.destination = connection.remote_host;
    header_.endpoint = connection.endpoint;
    return true;
}

static_assert(alignof(PacketBuffer) >= 4U, "PacketBuffer must be four-byte aligned");
static_assert(sizeof(PacketBuffer) == 12U, "Ingress metadata must not grow the packet prefix");
static_assert((sizeof(PacketBuffer) % 4U) == 0U, "Packet payload offset must be four-byte aligned");

}  // namespace wirespaces

#define WS_PACKET_BUFFER_DEFINE(name, payload_capacity)               \
    class alignas(4) name final : public ::wirespaces::PacketBuffer { \
    public:                                                           \
        static constexpr uint16_t kPayloadCapacity{payload_capacity}; \
                                                                      \
        constexpr name() noexcept : PacketBuffer{kPayloadCapacity} {} \
                                                                      \
    private:                                                          \
        uint8_t payload_storage_[kPayloadCapacity]{};                 \
    };                                                                \
    static_assert(alignof(name) >= 4U, #name " must be four-byte aligned")
