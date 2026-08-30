/**
 * @file header.h
 * @brief Canonical WireSpaces packet header and strongly typed identifiers.
 */

#pragma once

#include <wirespaces/core/ws_constants.h>

#include <cstdint>

namespace wirespaces {

struct HostId {
    uint8_t value;
    [[nodiscard]] constexpr bool isBroadcast() const noexcept {
        return value == kBroadcastHostValue;
    }
};

constexpr bool operator==(HostId lhs, HostId rhs) noexcept {
    return lhs.value == rhs.value;
}
constexpr bool operator!=(HostId lhs, HostId rhs) noexcept {
    return !(lhs == rhs);
}

struct WireNumber {
    uint8_t value;
};

constexpr bool operator==(WireNumber lhs, WireNumber rhs) noexcept {
    return lhs.value == rhs.value;
}
constexpr bool operator!=(WireNumber lhs, WireNumber rhs) noexcept {
    return !(lhs == rhs);
}

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

    [[nodiscard]] static constexpr EndpointAddress from(Namespace namespace_id,
                                                        uint16_t endpoint_id) noexcept {
        return EndpointAddress{
            static_cast<uint16_t>((static_cast<uint16_t>(namespace_id) << kNamespaceShift) |
                                  (endpoint_id & kEndpointIdMask))};
    }

    [[nodiscard]] constexpr Namespace namespaceId() const noexcept {
        return static_cast<Namespace>((value & kNamespaceMask) >> kNamespaceShift);
    }

    [[nodiscard]] constexpr uint16_t endpointId() const noexcept {
        return value & kEndpointIdMask;
    }

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
    QoS qos{QoS::kNormal};
    bool has_extensions{false};
    TransportType transport_type{TransportType::kSimple};
};

class WS_PACKED Header {
public:
    uint8_t control{};
    [[nodiscard]] QoS qos() const noexcept;
    [[nodiscard]] bool hasExtensions() const noexcept;
    [[nodiscard]] TransportType transportType() const noexcept;

    void setQoS(QoS qos) noexcept;
    void setHasExtensions(bool has_extensions) noexcept;
    void setTransportType(TransportType transport_type) noexcept;
    void setControlFields(ControlFields control_fields) noexcept;

    WireNumber wire{};
    HostId source{};
    HostId destination{};
    EndpointAddress endpoint{};
};

static_assert(sizeof(HostId) == 1U, "HostId must remain one byte");
static_assert(sizeof(WireNumber) == 1U, "WireNumber must remain one byte");
static_assert(sizeof(EndpointAddress) == 2U, "EndpointAddress must remain two bytes");
static_assert(sizeof(Header) == 6U, "Header must remain six bytes");

}  // namespace wirespaces
