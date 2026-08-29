/**
 * @file header.cpp
 * @brief Canonical WireSpaces packet header accessors.
 */

#include <core/header.h>

namespace wirespaces {
namespace {
constexpr uint8_t kQoSMask{0xC0U};
constexpr uint8_t kQoSShift{6U};
constexpr uint8_t kHasExtensionsMask{0x08U};
constexpr uint8_t kTransportTypeMask{0x07U};
}  // namespace

QoS Header::qos() const noexcept {
    return static_cast<QoS>((control & kQoSMask) >> kQoSShift);
}

bool Header::hasExtensions() const noexcept {
    return (control & kHasExtensionsMask) != 0U;
}

TransportType Header::transportType() const noexcept {
    return static_cast<TransportType>(control & kTransportTypeMask);
}

void Header::setQoS(QoS qos_value) noexcept {
    control = static_cast<uint8_t>((control & static_cast<uint8_t>(~kQoSMask)) |
                                   (static_cast<uint8_t>(qos_value) << kQoSShift));
}

void Header::setHasExtensions(bool has_extensions) noexcept {
    if (has_extensions) {
        control |= kHasExtensionsMask;
    } else {
        control &= static_cast<uint8_t>(~kHasExtensionsMask);
    }
}

void Header::setTransportType(TransportType transport_type) noexcept {
    control = static_cast<uint8_t>((control & static_cast<uint8_t>(~kTransportTypeMask)) |
                                   static_cast<uint8_t>(transport_type));
}

void Header::setControlFields(ControlFields control_fields) noexcept {
    setQoS(control_fields.qos);
    setHasExtensions(control_fields.has_extensions);
    setTransportType(control_fields.transport_type);
}

}  // namespace wirespaces
