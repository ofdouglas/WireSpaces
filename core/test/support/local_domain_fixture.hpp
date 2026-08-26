/**
 * @file local_domain_fixture.hpp
 * @brief Wires route table + dispatch table for end-to-end local-domain tests.
 */

#pragma once

#include "local_domain.h"
#include "ws_constants.h"

#include "support/constants.hpp"
#include "support/dispatch_recorder.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test::support {

class LocalDomainFixture : public DispatchTableFixture {
protected:
    void SetUp() override {
        DispatchTableFixture::SetUp();
        registerEndpoint(kReceiverEndpoint, recorder_.handle());
        forward_context_ = LocalDomainForwardContext{&dispatch_table_};
        route_entries_[0] = RouteTableEntry{WS_WIRE_LOCAL_DOMAIN, WS_EGRESS_SET_NONE};
        route_table_ = RouteTable{
            route_entries_,
            1U,
            local_domain_forward_impl,
            &forward_context_,
        };
    }

    DispatchResult forwardDomain(TestPacket& packet) {
        return local_domain_forward(&route_table_, asPacketBuffer(&packet));
    }

    LocalDomainForwardContext forward_context_{};
    RouteTableEntry route_entries_[1]{};
    RouteTable route_table_{route_entries_, 0U, nullptr, nullptr};
};

} // namespace wirespaces::test::support
