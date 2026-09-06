/**
 * @file packet.h
 * @brief WireSpaces packet format and fixed-storage packet buffer API.
 */

#pragma once

#include <wirespaces/core/ws_constants.h>
#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces {

struct HostId {
    uint8_t value;
    constexpr bool isBroadcast() const noexcept { return value == kBroadcastHostValue; }
};

constexpr bool operator==(HostId lhs, HostId rhs) noexcept { return lhs.value == rhs.value; }
constexpr bool operator!=(HostId lhs, HostId rhs) noexcept { return !(lhs == rhs); }

struct WireNumber {
    uint8_t value;
};

constexpr bool operator==(WireNumber lhs, WireNumber rhs) noexcept { return lhs.value == rhs.value; }
constexpr bool operator!=(WireNumber lhs, WireNumber rhs) noexcept { return !(lhs == rhs); }

enum class QoS : uint8_t {
    kCritical = 0U,
    kHigh = 1U,
    kNormal = 2U,
    kBackground = 3U,
};

enum class TransportType : uint8_t { kSimple = 0U, kBits, kNumTransportTypes };

enum class Namespace : uint8_t {
    kUser0 = 0U,
    kUser1 = 1U,
    kUser2 = 2U,
    kCommon = 3U,
};

struct EndpointAddress {
    uint16_t value;

    static constexpr EndpointAddress from(Namespace namespace_id, uint16_t endpoint_id) noexcept {
        return EndpointAddress{
            static_cast<uint16_t>((static_cast<uint16_t>(namespace_id) << kNamespaceShift) |
                                  (endpoint_id & kEndpointIdMask))};
    }

    constexpr Namespace namespaceId() const noexcept {
        return static_cast<Namespace>((value & kNamespaceMask) >> kNamespaceShift);
    }

    constexpr uint16_t endpointId() const noexcept { return value & kEndpointIdMask; }

private:
    static constexpr uint16_t kNamespaceMask{0xC000U};
    static constexpr uint16_t kNamespaceShift{14U};
    static constexpr uint16_t kEndpointIdMask{0x3FFFU};
};

constexpr bool operator==(EndpointAddress lhs, EndpointAddress rhs) noexcept {
    return lhs.value == rhs.value;
}
constexpr bool operator!=(EndpointAddress lhs, EndpointAddress rhs) noexcept {
    return !(lhs == rhs);
}

struct ControlFields {
    static constexpr ControlFields simple(QoS qos = QoS::kNormal) noexcept {
        return ControlFields{qos, false, TransportType::kSimple};
    }

    static constexpr ControlFields bits(QoS qos = QoS::kNormal) noexcept {
        return ControlFields{qos, false, TransportType::kBits};
    }

    static constexpr ControlFields defaultControlFields() noexcept { return simple(); }

    QoS qos{QoS::kNormal};
    bool has_extensions{false};
    TransportType transport_type{TransportType::kSimple};
};

/** Transport-independent address used to initialize locally originated traffic. */
struct ConnectionAddress {
    WireNumber wire{};
    HostId local_host{};
    HostId remote_host{};
    EndpointAddress endpoint{};
};

class WS_PACKED Header {
public:
    uint8_t control{};
    QoS qos() const noexcept;
    bool hasExtensions() const noexcept;
    TransportType transportType() const noexcept;

    void setQoS(QoS qos) noexcept;
    void setHasExtensions(bool has_extensions) noexcept;
    void setTransportType(TransportType transport_type) noexcept;
    void setControlFields(ControlFields control_fields) noexcept;

    WireNumber wire{};
    HostId source{};
    HostId destination{};
    EndpointAddress endpoint{};
};

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
static_assert(sizeof(HostId) == 1U, "HostId must remain one byte");
static_assert(sizeof(WireNumber) == 1U, "WireNumber must remain one byte");
static_assert(sizeof(EndpointAddress) == 2U, "EndpointAddress must remain two bytes");
static_assert(sizeof(Header) == 6U, "Header must remain six bytes");
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
