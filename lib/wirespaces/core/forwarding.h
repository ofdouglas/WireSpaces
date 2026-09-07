/**
 * @file forwarding.h
 * @brief Link admission and coarse synchronous forwarding contracts.
 */

#pragma once

#include <wirespaces/core/packet.h>

namespace wirespaces {

/// @brief Which of this host's network interfaces a wire connects to.
using InterfaceSet = uint8_t;
constexpr InterfaceSet kNoEgress{0U};

/** @brief Local admission only, never remote delivery. */
enum class LinkAdmission : uint8_t { kAccepted, kFull, kTooLarge, kRejected };

/** @brief Coarse forwarding outcome; partial acceptance is neither rolled back nor retried.
 * Full means no acceptance and only temporary congestion. With no acceptance,
 * rejection takes precedence over size failure, which takes precedence over Full.
 * NoEgress is normal at an incoming leaf; NoRoute/InvalidIngress reject routing.
 */
enum class RouteResult : uint8_t {
    kAccepted, kPartial, kFull, kTooLarge, kRejected,
    kNoRoute, kNoEgress, kInvalidIngress,
};

/** @brief Link-local admission. Do not borrow caller-owned packet storage after return.
 * Driver-context routing requires context-safe, bounded implementations.
 */
class PacketLink {
public:
    virtual LinkAdmission trySend(const PacketBuffer& packet) noexcept = 0;
protected:
    ~PacketLink() = default;
};

/** @brief Attempt each selected Link once, without logging, retries or rollback.
 * Return a coarse admission outcome, not per-interface diagnostics.
 * Empty physical fan-out is accepted; local-domain forwarders report local admission.
 */
class PacketForwarder {
public:
    virtual RouteResult forward(const PacketBuffer& packet, InterfaceSet egress_set) noexcept = 0;
protected:
    ~PacketForwarder() = default;
};

/** @brief Fold one Link outcome into fan-out admission; start with NoEgress.
 * Only admission outcomes or the initial NoEgress value may be passed as current.
 */
inline RouteResult combineAdmission(RouteResult current, LinkAdmission admission) noexcept {
    RouteResult next{RouteResult::kRejected};
    switch (admission) {
        case LinkAdmission::kAccepted: next = RouteResult::kAccepted; break;
        case LinkAdmission::kFull: next = RouteResult::kFull; break;
        case LinkAdmission::kTooLarge: next = RouteResult::kTooLarge; break;
        case LinkAdmission::kRejected: break;
    }
    if (current == RouteResult::kNoEgress || current == next) return next;
    if (current == RouteResult::kPartial || current == RouteResult::kAccepted ||
        next == RouteResult::kAccepted) return RouteResult::kPartial;
    if (current == RouteResult::kRejected || next == RouteResult::kRejected) return RouteResult::kRejected;
    return RouteResult::kTooLarge;
}

} // namespace wirespaces
