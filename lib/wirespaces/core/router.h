/**
 * @file router.h
 * @brief Static WireSpaces route table and packet forwarding interface.
 */

#pragma once

#include <wirespaces/core/packet.h>
#include <wirespaces/core/dispatch.h>
#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces {

using EgressSet = uint8_t;
constexpr EgressSet kNoEgress{0U};

struct RouteTableEntry {
    WireNumber wire{};
    // Selected local attachments: origin fan-out and admitted ingress interfaces.
    EgressSet egress_set{kNoEgress};
};

class PacketForwarder {
public:
    virtual void forward(const PacketBuffer& packet, EgressSet egress_set) noexcept = 0;

protected:
    ~PacketForwarder() = default;
};

enum class RouteResult : uint8_t {
    kForwarded = 0U,
    kNoRoute,
    kNoEgress,
    kInvalidIngress,
};

struct IngressResult {
    RouteResult routing{RouteResult::kNoRoute};
    DispatchResult delivery{DispatchResult::kNoEndpoint};
};

/** @brief Route over an acyclic configured realization; synchronization belongs to Links/receivers. */
class Router {
public:
    constexpr Router(foundation::Span<const RouteTableEntry> entries,
                     PacketForwarder& forwarder) noexcept
        : entries_{entries}, forwarder_{forwarder} {}

    /** @brief Forward local-origin traffic, or validate and exclude a tagged ingress interface.
     * Does not deliver locally. Received traffic at a leaf returns kNoEgress without a callback.
     * Local-origin zero masks still reach the forwarder for local-domain compatibility.
     */
    RouteResult forward(const PacketBuffer& packet) const noexcept;

    /** @brief Legacy ingress using process-global identity, stamping ingress index (1..8).
     * Reject unknown routes/unselected ingress before any forwarding or delivery.
     * Local delivery additionally requires Wire membership and Dispatcher destination checks.
     * Packet storage is borrowed; forwarders and receivers must copy/enqueue before returning.
     * May run in driver or router-task context; caller supplies any required synchronization.
     */
    IngressResult receive(PacketBuffer& packet, uint8_t ingress_index,
                                        const Dispatcher& dispatcher) const noexcept;
    /** @brief Explicit-domain ingress; membership and destination checks use host. */
    IngressResult receive(PacketBuffer& packet, uint8_t ingress_index,
                                        const Dispatcher& dispatcher, const HostInfo& host) const noexcept;

private:
    foundation::Span<const RouteTableEntry> entries_{};
    PacketForwarder& forwarder_;
};

}  // namespace wirespaces
