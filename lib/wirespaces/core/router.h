/**
 * @file router.h
 * @brief Static WireSpaces route table and packet forwarding interface.
 */

#pragma once

#include <wirespaces/core/packet.h>
#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces {

using EgressSet = uint8_t;
constexpr EgressSet kNoEgress{0U};

struct RouteTableEntry {
    WireNumber wire{};
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
};

class Router {
public:
    constexpr Router(foundation::Span<const RouteTableEntry> entries,
                     PacketForwarder& forwarder) noexcept
        : entries_{entries}, forwarder_{forwarder} {}

    [[nodiscard]] RouteResult forward(const PacketBuffer& packet) const noexcept;

private:
    foundation::Span<const RouteTableEntry> entries_{};
    PacketForwarder& forwarder_;
};

}  // namespace wirespaces
