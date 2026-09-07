/**
 * @file dispatch.h
 * @brief Endpoint receiver interface and bounded dispatch table.
 */

#pragma once

#include <wirespaces/core/packet.h>
#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces {
struct HostInfo;

enum class ReceiveResult : uint8_t {
    kAccepted = 0U,
    kFull,
    kRejected,
};

class EndpointReceiver {
public:
    virtual ReceiveResult receive(const PacketBuffer& packet) noexcept = 0;

protected:
    ~EndpointReceiver() = default;
};

/** @brief Admit only supported controls for the receiver's declared transport.
 * The wrapped receiver must outlive this adapter and supplies storage/synchronization.
 * Validation happens before admission, including when called without a Dispatcher.
 */
class TransportFilterReceiver final : public EndpointReceiver {
public:
    constexpr TransportFilterReceiver(EndpointReceiver& receiver, TransportType transport) noexcept
        : receiver_{receiver}, transport_{transport} {}

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;

private:
    EndpointReceiver& receiver_;
    const TransportType transport_;
};

inline ReceiveResult TransportFilterReceiver::receive(const PacketBuffer& packet) noexcept {
    const Header& header{packet.header()};
    if (!header.hasSupportedControl() || header.transportType() != transport_) {
        return ReceiveResult::kRejected;
    }
    return receiver_.receive(packet);
}

struct DispatchTableEntry {
    EndpointAddress endpoint{};
    EndpointReceiver* receiver{nullptr};
};

enum class DispatchResult : uint8_t {
    kAccepted = 0U,
    kFull,
    kRejected,
    kNoEndpoint,
};

class Dispatcher {
public:
    explicit constexpr Dispatcher(foundation::Span<const DispatchTableEntry> entries) noexcept
        : entries_{entries} {}

    /** @brief Legacy dispatch using process-global host identity. */
    DispatchResult dispatch(const PacketBuffer& packet) const noexcept;
    /** @brief Dispatch with explicit identity; unsupported controls are rejected before any receiver.
     * Endpoint owners additionally enforce their required transport (e.g. TransportFilterReceiver).
     */
    DispatchResult dispatch(const PacketBuffer& packet, const HostInfo& host) const noexcept;

private:
    foundation::Span<const DispatchTableEntry> entries_{};
};

}  // namespace wirespaces
