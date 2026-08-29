/**
 * @file local_domain.h
 * @brief Router forwarder that delivers packets into a local Dispatcher.
 */

#pragma once

#include <core/dispatch.h>
#include <core/router.h>

namespace wirespaces {

class LocalDomainForwarder final : public PacketForwarder {
public:
    explicit constexpr LocalDomainForwarder(Dispatcher& dispatcher) noexcept
        : dispatcher_{dispatcher} {}

    void forward(const PacketBuffer& packet, EgressSet egress_set) noexcept override;

    [[nodiscard]] DispatchResult lastResult() const noexcept {
        return last_result_;
    }

private:
    Dispatcher& dispatcher_;
    DispatchResult last_result_{DispatchResult::kNoEndpoint};
};

}  // namespace wirespaces
