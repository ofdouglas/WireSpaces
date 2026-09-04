/**
 * @file endpoint_queue.h
 * @brief Fixed-capacity queued EndpointReceiver.
 */

#pragma once

#include <wirespaces/containers/queue.h>
#include <wirespaces/core/dispatch.h>
#include <wirespaces/foundation/array.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wirespaces {

/**
 * @brief Copies accepted packets into FIFO storage for later polling.
 *
 * Dispatch only validates and copies a packet. Service code calls dequeue()
 * later from its own execution context. LockType determines how concurrent
 * framework writers are serialized; NoLock is suitable for a serialized main
 * loop or other single-writer domain.
 *
 * @tparam kPayloadCapacity Maximum payload bytes retained per packet.
 * @tparam kQueueCapacity Number of complete packets retained by the Endpoint.
 * @tparam LockType Lock policy used by the underlying bounded queue.
 */
template <std::size_t kPayloadCapacity, std::size_t kQueueCapacity,
          typename LockType = containers::NoLock>
class EndpointReceiverQueue final : public EndpointReceiver {
public:
    static_assert(kPayloadCapacity <= UINT16_MAX,
                  "Endpoint payload capacity must fit in uint16_t");
    static_assert(kQueueCapacity > 0U,
                  "Endpoint queue capacity must be greater than zero");

    explicit EndpointReceiverQueue(LockType lock = LockType{}) noexcept;

    /// @brief Copy one complete packet into the queue.
    ReceiveResult receive(const PacketBuffer& packet) noexcept override;

    /**
     * @brief Remove the oldest packet and copy it into caller-owned storage.
     *
     * The output buffer must have at least kPayloadCapacity bytes of payload
     * capacity. A false result leaves the output unchanged.
     */
    [[nodiscard]] bool dequeue(PacketBuffer& output) noexcept;

private:
    struct StoredPacket {
        Header header{};
        foundation::Array<uint8_t, kPayloadCapacity> payload{};
        uint16_t size{0U};
    };

    containers::Queue<StoredPacket, kQueueCapacity, LockType> queue_;
};

// --- EndpointReceiverQueue implementations ---

template <std::size_t kPayloadCapacity, std::size_t kQueueCapacity,
          typename LockType>
EndpointReceiverQueue<kPayloadCapacity, kQueueCapacity, LockType>::EndpointReceiverQueue(
    LockType lock) noexcept
    : queue_{static_cast<LockType&&>(lock)} {}

template <std::size_t kPayloadCapacity, std::size_t kQueueCapacity,
          typename LockType>
ReceiveResult EndpointReceiverQueue<kPayloadCapacity, kQueueCapacity, LockType>::receive(
    const PacketBuffer& packet) noexcept {
    if (packet.size() > kPayloadCapacity) {
        return ReceiveResult::kRejected;
    }

    StoredPacket stored{};
    stored.header = packet.header();
    stored.size = packet.size();
    if (stored.size > 0U) {
        std::memcpy(stored.payload.data(), packet.payload().data(), stored.size);
    }
    return queue_.enqueue(stored) ? ReceiveResult::kAccepted : ReceiveResult::kFull;
}

template <std::size_t kPayloadCapacity, std::size_t kQueueCapacity,
          typename LockType>
bool EndpointReceiverQueue<kPayloadCapacity, kQueueCapacity, LockType>::dequeue(
    PacketBuffer& output) noexcept {
    if (output.capacity() < kPayloadCapacity) {
        return false;
    }

    StoredPacket stored{};
    if (!queue_.dequeue(stored)) {
        return false;
    }

    static_cast<void>(output.resize(stored.size));
    output.header() = stored.header;
    if (stored.size > 0U) {
        std::memcpy(output.payload().data(), stored.payload.data(), stored.size);
    }
    return true;
}

}  // namespace wirespaces
