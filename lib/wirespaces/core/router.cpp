/**
 * @file router.cpp
 * @brief Static WireSpaces router implementation.
 */

#include <wirespaces/core/router.h>
#include <wirespaces/core/host.h>

namespace wirespaces {

RouteResult Router::forward(const PacketBuffer& packet) const noexcept {
    const uint8_t ingress{packet.ingressIndex()};
    if (ingress > 8U) {
        return RouteResult::kInvalidIngress;
    }
    const InterfaceSet ingress_mask{ingress == 0U ? InterfaceSet{0U} : static_cast<InterfaceSet>(1U << (ingress - 1U))};
    for (const RouteTableEntry& entry : entries_) {
        if (entry.wire != packet.header().wire) {
            continue;
        }
        if ((ingress_mask != 0U) && ((entry.wire_interfaces & ingress_mask) == 0U)) {
            return RouteResult::kInvalidIngress;
        }
        const InterfaceSet egress{static_cast<InterfaceSet>(entry.wire_interfaces & ~ingress_mask)};
        if ((egress == kNoEgress) && (ingress != 0U)) {
            return RouteResult::kNoEgress;
        }
        return forwarder_.forward(packet, egress);
    }
    return RouteResult::kNoRoute;
}

IngressResult Router::receive(PacketBuffer& packet, uint8_t ingress_index,
                              const Dispatcher& dispatcher) const noexcept {
    return receive(packet, ingress_index, dispatcher, localHostInfo());
}

IngressResult Router::receive(PacketBuffer& packet, uint8_t ingress_index,
                              const Dispatcher& dispatcher, const HostInfo& host) const noexcept {
    packet.setIngressIndex(ingress_index);
    if (ingress_index == 0U) {
        return {RouteResult::kInvalidIngress, DispatchResult::kNoEndpoint};
    }
    const RouteResult routing{forward(packet)};
    if ((routing == RouteResult::kNoRoute || routing == RouteResult::kInvalidIngress) ||
        !host.isMemberOf(packet.header().wire)) {
        return {routing, DispatchResult::kNoEndpoint};
    }
    return {routing, dispatcher.dispatch(packet, host)};
}

}  // namespace wirespaces
