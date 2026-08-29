/**
 * @file local_domain.cpp
 * @brief Local-domain router forwarder implementation.
 */

#include <wirespaces/core/local_domain.h>

namespace wirespaces {

void LocalDomainForwarder::forward(const PacketBuffer& packet, EgressSet egress_set) noexcept {
    static_cast<void>(egress_set);
    last_result_ = dispatcher_.dispatch(packet);
}

}  // namespace wirespaces
