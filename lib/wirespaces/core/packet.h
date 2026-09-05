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
    [[nodiscard]] uint16_t capacity() const noexcept {
        return capacity_;
    }
    // Size of the payload.
    [[nodiscard]] uint16_t size() const noexcept {
        return size_;
    }
    // Size of the packet including the header.
    [[nodiscard]] uint16_t totalSize() const noexcept {
        return size_ + sizeof(Header);
    }

    // Host-local metadata, never serialized: zero means local origin; 1..8
    // identify the receiving interface by its egress bit + 1.
    [[nodiscard]] uint8_t ingressIndex() const noexcept { return ingress_index_; }
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
    [[nodiscard]] bool initializeResponseTo(const Header& request_header, uint16_t size, ControlFields control_fields) noexcept;  

    [[nodiscard]] Header& header() noexcept {
        return header_;
    }
    [[nodiscard]] const Header& header() const noexcept {
        return header_;
    }

    [[nodiscard]] MutableByteSpan payload() noexcept;
    [[nodiscard]] ByteSpan payload() const noexcept;
    [[nodiscard]] ByteSpan headerAndPayload() const noexcept;

protected:
    explicit constexpr PacketBuffer(uint16_t capacity) noexcept : capacity_{capacity} {}

private:
    uint16_t capacity_{0U};
    uint16_t size_{0U};
    uint8_t ingress_index_{0U};
    uint8_t payload_alignment_padding_{0U};
    Header header_{};
};

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
