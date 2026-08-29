/**
 * @file router_spy.hpp
 * @brief Records router forward callbacks for route-table unit tests.
 */

#pragma once

#include <cstdint>

#include <runtime/core.hpp>

#include "support/host_fixture.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test::support {

class RouterForwardSpy {
public:
    void reset() {
        called_ = false;
        last_packet_ = nullptr;
        last_egress_set_ = 0U;
    }

    static void forward(void* context, const PacketBuffer* packet, EgressSet egress_set) {
        auto* spy = static_cast<RouterForwardSpy*>(context);
        spy->called_ = true;
        spy->last_packet_ = packet;
        spy->last_egress_set_ = egress_set;
    }

    bool called() const {
        return called_;
    }

    const PacketBuffer* lastPacket() const {
        return last_packet_;
    }

    EgressSet lastEgressSet() const {
        return last_egress_set_;
    }

private:
    bool called_{false};
    const PacketBuffer* last_packet_{nullptr};
    EgressSet last_egress_set_{0U};
};

class RouteTableFixture : public DefaultHostFixture {
protected:
    void SetUp() override {
        DefaultHostFixture::SetUp();
        forward_spy_.reset();
        route_table_ = RouteTable{route_entries_, 0U, nullptr, nullptr};
    }

    void buildRouteTable(uint8_t wire_number, EgressSet egress_set) {
        route_entries_[0] = RouteTableEntry{wire_number, egress_set};
        route_table_ = RouteTable{
            route_entries_,
            1U,
            RouterForwardSpy::forward,
            &forward_spy_,
        };
    }

    DispatchResult forward(TestPacket& packet) {
        return ws_router_forward_packet(&route_table_, asPacketBuffer(&packet));
    }

    RouterForwardSpy forward_spy_{};
    RouteTableEntry route_entries_[1]{};
    RouteTable route_table_{route_entries_, 0U, nullptr, nullptr};
};

} // namespace wirespaces::test::support
