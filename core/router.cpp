/**
 * @file router.cpp
 * @brief Static WireSpaces router implementation.
 */

#include <core/router.h>

namespace wirespaces {

RouteResult Router::forward(const PacketBuffer& packet) const noexcept {
    for (const RouteTableEntry& entry : entries_) {
        if (entry.wire != packet.header().wire) {
            continue;
        }
        forwarder_.forward(packet, entry.egress_set);
        return RouteResult::kForwarded;
    }
    return RouteResult::kNoRoute;
}

}  // namespace wirespaces
