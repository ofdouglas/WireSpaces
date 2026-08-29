/**
 * @file mailbox.cpp
 * @brief Single-slot snapshot EndpointReceiver implementation.
 */

#include <wirespaces/core/mailbox.h>

#include <cstring>

namespace wirespaces {

ReceiveResult EndpointSnapshotReceiver::receive(const PacketBuffer& packet) noexcept {
    const ByteSpan input{packet.payload()};
    if (input.size() > sizeof(data_)) {
        return ReceiveResult::kRejected;
    }

    if (!input.empty()) {
        std::memcpy(data_, input.data(), input.size());
    }
    length_ = static_cast<uint16_t>(input.size());
    occupied_ = true;
    ++generation_;
    return ReceiveResult::kAccepted;
}

bool EndpointSnapshotReceiver::read(MutableByteSpan output, uint16_t& length,
                                    uint32_t& generation_value) const noexcept {
    if (!occupied_ || length_ > output.size()) {
        return false;
    }

    if (length_ > 0U) {
        std::memcpy(output.data(), data_, length_);
    }
    length = length_;
    generation_value = generation_;
    return true;
}

}  // namespace wirespaces
