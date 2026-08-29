/**
 * @file packet.cpp
 * @brief Fixed-storage WireSpaces packet buffer implementation.
 */

#include <wirespaces/core/packet.h>

namespace wirespaces {

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

}  // namespace wirespaces
