/**
 * @file router_spy.hpp
 * @brief PacketForwarder and route fixture for router unit tests.
 */

#pragma once

#include <wirespaces/runtime/core.hpp>

#include "support/host_fixture.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test::support {

class RouterForwardSpy final : public PacketForwarder {
public:
    void forward(const PacketBuffer& packet, EgressSet egress_set) noexcept override {
        called_ = true;
        last_packet_ = &packet;
        last_egress_set_ = egress_set;
    }

    void reset() noexcept {
        called_ = false;
        last_packet_ = nullptr;
        last_egress_set_ = kNoEgress;
    }

    bool called() const noexcept { return called_; }
    const PacketBuffer* lastPacket() const noexcept { return last_packet_; }
    EgressSet lastEgressSet() const noexcept { return last_egress_set_; }

private:
    bool called_{false};
    const PacketBuffer* last_packet_{nullptr};
    EgressSet last_egress_set_{kNoEgress};
};

class RouteTableFixture : public DefaultHostFixture {
protected:
    void SetUp() override {
        DefaultHostFixture::SetUp();
        forward_spy_.reset();
        route_entry_ = RouteTableEntry{kLocalWire, kNoEgress};
    }

    RouteResult forward(TestPacket& packet) const {
        return router_.forward(packet);
    }

    RouterForwardSpy forward_spy_{};
    RouteTableEntry route_entry_{};
    Router router_{foundation::Span<const RouteTableEntry>{&route_entry_, 1U}, forward_spy_};
};

} // namespace wirespaces::test::support
