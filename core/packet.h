/**
 * @file packet.h
 * @brief Fixed-storage, non-templated WireSpaces packet buffer API.
 */

#pragma once

#include <core/header.h>
#include <foundation/span.h>

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
    [[nodiscard]] uint16_t capacity() const noexcept {
        return capacity_;
    }
    [[nodiscard]] uint16_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool resize(uint16_t size) noexcept;
    [[nodiscard]] bool initialize(uint16_t size, ControlFields control_fields) noexcept;

    [[nodiscard]] Header& header() noexcept {
        return header_;
    }
    [[nodiscard]] const Header& header() const noexcept {
        return header_;
    }

    [[nodiscard]] MutableByteSpan payload() noexcept;
    [[nodiscard]] ByteSpan payload() const noexcept;

protected:
    explicit constexpr PacketBuffer(uint16_t capacity) noexcept : capacity_{capacity} {}

private:
    uint16_t capacity_{0U};
    uint16_t size_{0U};
    uint16_t payload_alignment_padding_{0U};
    Header header_{};
};

static_assert(alignof(PacketBuffer) >= 4U, "PacketBuffer must be four-byte aligned");
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
