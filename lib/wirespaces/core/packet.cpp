/**
 * @file packet.cpp
 * @brief Fixed-storage WireSpaces packet buffer implementation.
 */

#include <wirespaces/core/packet.h>
#include <cstring>

namespace wirespaces {

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
