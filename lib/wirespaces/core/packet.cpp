/**
 * @file packet.cpp
 * @brief Fixed-storage WireSpaces packet buffer implementation.
 */

#include <wirespaces/core/packet.h>
#include <cstring>

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

bool PacketBuffer::copyFrom(const PacketBuffer& source) noexcept {
    if (source.size_ > capacity_) {
        return false;
    }
    if (this == &source) {
        return true;
    }
    header_ = source.header_;
    size_ = source.size_;
    ingress_index_ = source.ingress_index_;
    if (size_ > 0U) {
        std::memcpy(payload().data(), source.payload().data(), size_);
    }
    return true;
}

bool PacketBuffer::resize(uint16_t new_size) noexcept {
    if (new_size > capacity_) {
        return false;
    }
    size_ = new_size;
    return true;
}

bool PacketBuffer::initialize(uint16_t new_size, ControlFields control_fields) noexcept {
    if (!resize(new_size)) {
        return false;
    }
    header_.setControlFields(control_fields);
    ingress_index_ = 0U;
    return true;
}

bool PacketBuffer::initializeResponseTo(const Header& request_header, uint16_t size, ControlFields control_fields) noexcept {
    if (!initialize(size, control_fields)) {
        return false;
    }
    header_.wire = request_header.wire;
    header_.source = request_header.destination;
    header_.destination = request_header.source;
    header_.endpoint = request_header.endpoint;
    return true;
}

MutableByteSpan PacketBuffer::payload() noexcept {
    auto* bytes = reinterpret_cast<uint8_t*>(this) + sizeof(PacketBuffer);
    return MutableByteSpan{bytes, size_};
}

ByteSpan PacketBuffer::payload() const noexcept {
    const auto* bytes = reinterpret_cast<const uint8_t*>(this) + sizeof(PacketBuffer);
    return ByteSpan{bytes, size_};
}

ByteSpan PacketBuffer::headerAndPayload() const noexcept {
    return ByteSpan{reinterpret_cast<const uint8_t*>(&header()), totalSize()};
}

}  // namespace wirespaces
