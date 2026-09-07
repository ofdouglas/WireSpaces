/**
 * @file dispatch.cpp
 * @brief Endpoint dispatch implementation.
 */

#include <wirespaces/core/dispatch.h>
#include <wirespaces/core/host.h>

namespace wirespaces {
namespace {

DispatchResult toDispatchResult(ReceiveResult result) noexcept {
    switch (result) {
        case ReceiveResult::kAccepted:
            return DispatchResult::kAccepted;
        case ReceiveResult::kFull:
            return DispatchResult::kFull;
        case ReceiveResult::kRejected:
            return DispatchResult::kRejected;
    }
    return DispatchResult::kRejected;
}

uint8_t resultPriority(DispatchResult result) noexcept {
    switch (result) {
        case DispatchResult::kAccepted:
            return 3U;
        case DispatchResult::kFull:
            return 2U;
        case DispatchResult::kRejected:
            return 1U;
        case DispatchResult::kNoEndpoint:
            return 0U;
    }
    return 0U;
}

}  // namespace

DispatchResult Dispatcher::dispatch(const PacketBuffer& packet) const noexcept {
    return dispatch(packet, localHostInfo());
}

DispatchResult Dispatcher::dispatch(const PacketBuffer& packet, const HostInfo& host) const noexcept {
    const Header& header{packet.header()};
    if (!header.hasSupportedControl()) {
        return DispatchResult::kRejected;
    }
    const bool broadcast{header.destination.isBroadcast()};
    if (broadcast && !host.isMemberOf(header.wire)) {
        return DispatchResult::kNoEndpoint;
    }

    DispatchResult aggregate{DispatchResult::kNoEndpoint};
    for (const DispatchTableEntry& entry : entries_) {
        if ((entry.endpoint != header.endpoint) || (entry.receiver == nullptr)) {
            continue;
        }
        if (!broadcast && (host.id != header.destination)) {
            continue;
        }

        const DispatchResult result{toDispatchResult(entry.receiver->receive(packet))};
        if (!broadcast) {
            return result;
        }
        if (resultPriority(result) > resultPriority(aggregate)) {
            aggregate = result;
        }
    }

    return aggregate;
}

}  // namespace wirespaces
