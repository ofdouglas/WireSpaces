/**
 * @file domain.cpp
 * @brief Local-domain forwarding implementation.
 */

#include <wirespaces/core/domain.h>

namespace wirespaces {

RouteResult LocalDispatchForwarder::forward(const PacketBuffer& packet, InterfaceSet egress_set) noexcept {
    static_cast<void>(egress_set);
    last_result_ = dispatcher_.dispatch(packet);
    return last_result_ == DispatchResult::kAccepted ? RouteResult::kAccepted :
        last_result_ == DispatchResult::kFull ? RouteResult::kFull : RouteResult::kRejected;
}

}  // namespace wirespaces
