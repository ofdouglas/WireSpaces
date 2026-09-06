/**
 * @file domain.cpp
 * @brief Local-domain forwarding implementation.
 */

#include <wirespaces/core/domain.h>

namespace wirespaces {

void LocalDispatchForwarder::forward(const PacketBuffer& packet, EgressSet egress_set) noexcept {
    static_cast<void>(egress_set);
    last_result_ = dispatcher_.dispatch(packet);
}

}  // namespace wirespaces
