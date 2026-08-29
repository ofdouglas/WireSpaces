/**
 * @file dispatch.h
 * @brief Endpoint receiver interface and bounded dispatch table.
 */

#pragma once

#include <wirespaces/core/packet.h>
#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces {

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

struct DispatchTableEntry {
    HostId host{};
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

    [[nodiscard]] DispatchResult dispatch(const PacketBuffer& packet) const noexcept;

private:
    foundation::Span<const DispatchTableEntry> entries_{};
};

}  // namespace wirespaces
