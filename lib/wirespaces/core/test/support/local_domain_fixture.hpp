/**
 * @file local_domain_fixture.hpp
 * @brief Router to Dispatcher local-domain integration fixture.
 */

#pragma once

#include <wirespaces/runtime/core.hpp>

#include "support/constants.hpp"
#include "support/dispatch_recorder.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test::support {

class LocalDomainFixture : public DefaultHostFixture {
protected:
    void SetUp() override {
        DefaultHostFixture::SetUp();
        recorder_.reset();
        dispatch_entry_ = DispatchTableEntry{kLocalHostId, kReceiverEndpoint, &recorder_};
        route_entry_ = RouteTableEntry{kLocalWire, kNoEgress};
    }

    [[nodiscard]] RouteResult forwardDomain(TestPacket& packet) const {
        return router_.forward(packet);
    }

    DispatchRecorder recorder_{};
    DispatchTableEntry dispatch_entry_{};
    Dispatcher dispatcher_{foundation::Span<const DispatchTableEntry>{&dispatch_entry_, 1U}};
    LocalDomainForwarder local_forwarder_{dispatcher_};
    RouteTableEntry route_entry_{};
    Router router_{foundation::Span<const RouteTableEntry>{&route_entry_, 1U}, local_forwarder_};
};

} // namespace wirespaces::test::support
