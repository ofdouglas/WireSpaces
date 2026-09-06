/**
 * @file domain.h
 * @brief Explicit domain context and local-dispatch forwarding support.
 */

#pragma once

#include <wirespaces/core/host.h>
#include <wirespaces/core/router.h>

namespace wirespaces {

/** @brief Publication scope and destination; the service supplies its endpoint. */
struct PublicationConfig {
    WireNumber wire{};
    HostId destination{};
};

/**
 * @brief Own immutable local identity and a Router, without process-global registration.
 * Routes and the forwarder must outlive this context; consumers must not outlive it.
 * Links, services and dispatch entries remain application-owned. No synchronization is added.
 */
class DomainContext final {
public:
    constexpr DomainContext(HostInfo host, foundation::Span<const RouteTableEntry> routes, PacketForwarder& forwarder) noexcept
        : host_{host}, router_{routes, forwarder} {}
        
    DomainContext(const DomainContext&) = delete;
    DomainContext& operator=(const DomainContext&) = delete;
    DomainContext(DomainContext&&) = delete;
    DomainContext& operator=(DomainContext&&) = delete;

    const HostInfo& hostInfo() const noexcept { return host_; }
    
    Router& router() noexcept { return router_; }
    
    const Router& router() const noexcept { return router_; }

    /** @brief Bind application publication settings to this domain's source identity. */
    constexpr ConnectionAddress connection(PublicationConfig publication, EndpointAddress endpoint) const noexcept {
        return {publication.wire, host_.id, publication.destination, endpoint};
    }

    /** @brief Route ingress and dispatch using this context's identity, never the global host. */
    IngressResult receive(PacketBuffer& packet, uint8_t ingress_index, const Dispatcher& dispatcher) const noexcept {
        return router_.receive(packet, ingress_index, dispatcher, host_);
    }

private:
    const HostInfo host_;
    Router router_;
};

/** @brief Router forwarder that delivers packets into a local Dispatcher. */
class LocalDispatchForwarder final : public PacketForwarder {
public:
    explicit constexpr LocalDispatchForwarder(Dispatcher& dispatcher) noexcept
        : dispatcher_{dispatcher} {}

    void forward(const PacketBuffer& packet, EgressSet egress_set) noexcept override;

    DispatchResult lastResult() const noexcept { return last_result_; }

private:
    Dispatcher& dispatcher_;
    DispatchResult last_result_{DispatchResult::kNoEndpoint};
};

}  // namespace wirespaces
