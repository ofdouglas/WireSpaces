/**
 * @file mailbox.h
 * @brief Single-slot snapshot EndpointReceiver.
 */

#pragma once

#include <wirespaces/core/dispatch.h>
#include <wirespaces/core/ws_constants.h>

#include <cstdint>

namespace wirespaces {

/**
 * @brief Retains the latest received payload and its generation.
 *
 * Synchronization is deliberately internal to the receiver implementation.
 * The initial implementation is intended for a serialized single-writer domain.
 */
class EndpointSnapshotReceiver final : public EndpointReceiver {
public:
    ReceiveResult receive(const PacketBuffer& packet) noexcept override;

    [[nodiscard]] bool read(MutableByteSpan output, uint16_t& length,
                            uint32_t& generation) const noexcept;

    bool hasValue() const noexcept {
        return occupied_;
    }
    uint32_t generation() const noexcept {
        return generation_;
    }

private:
    uint8_t data_[kDefaultEndpointStorageCapacity]{};
    uint16_t length_{0U};
    bool occupied_{false};
    uint32_t generation_{0U};
};

}  // namespace wirespaces
