/*
 * @file  pdu_adapter.hpp
 * @brief PDU adapter for the CAN classic protocol.
 */

#pragma once

#include <wirespaces/crc/crc_algorithm.h>
#include <wirespaces/foundation/span.h>

#include <cstddef>
#include <cstdint>

namespace wirespaces::links::can_classic {

// Receives messages composed of [1, MaxNumFrames] CAN frames
// TODO: CAN ID filtering. Only 1 ID can be accepted at a time.
// TODO: buffer timeout / abandon after message loss / newer generation received
template <size_t MaxNumFrames = 4U>
class PduaReceiver {
public:
    // Receive a CAN frame and add it to the buffer.
    // Returns true if a complete message is ready.
    bool receive(const Frame& frame) noexcept;

    // TODO: need to provide a WS packet -- reconstruct the original PDU from CAN ID & frame data
    bool getMessage(foundation::Span<const std::uint8_t> message) noexcept;

private:
    wirespaces::foundation::array<Frame, MaxNumFrames> buffer_{};
};

class PduAdapter {
public:
    PduAdapter() noexcept;
    ~PduAdapter() noexcept;

    void process(const foundation::Span<const std::uint8_t>& pdu) noexcept;
};

}  // namespace wirespaces::links::can_classic