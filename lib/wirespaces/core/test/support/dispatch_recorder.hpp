/**
 * @file dispatch_recorder.hpp
 * @brief EndpointReceiver and dispatch fixture for unit tests.
 */

#pragma once

#include <cstdint>

#include <wirespaces/runtime/core.hpp>

#include "support/host_fixture.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test::support {

class DispatchRecorder final : public EndpointReceiver {
public:
    ReceiveResult receive(const PacketBuffer& packet) noexcept override {
        ++invocation_count_;
        last_packet_ = &packet;
        return result_;
    }

    void reset() noexcept {
        invocation_count_ = 0U;
        last_packet_ = nullptr;
        result_ = ReceiveResult::kAccepted;
    }

    void setResult(ReceiveResult result) noexcept { result_ = result; }
    uint32_t invocationCount() const noexcept { return invocation_count_; }
    const PacketBuffer* lastPacket() const noexcept { return last_packet_; }

private:
    uint32_t invocation_count_{0U};
    const PacketBuffer* last_packet_{nullptr};
    ReceiveResult result_{ReceiveResult::kAccepted};
};

class DispatchTableFixture : public DefaultHostFixture {
protected:
    void SetUp() override {
        DefaultHostFixture::SetUp();
        entry_count_ = 0U;
        recorder_.reset();
    }

    void registerEndpoint(EndpointAddress endpoint,
                          EndpointReceiver& receiver) {
        dispatch_entries_[entry_count_] =
            DispatchTableEntry{endpoint, &receiver};
        ++entry_count_;
    }

    DispatchResult dispatch(TestPacket& packet) const {
        const Dispatcher dispatcher{
            foundation::Span<const DispatchTableEntry>{dispatch_entries_, entry_count_}};
        return dispatcher.dispatch(packet);
    }

    DispatchRecorder recorder_{};
    DispatchTableEntry dispatch_entries_[4]{};
    size_t entry_count_{0U};
};

} // namespace wirespaces::test::support
