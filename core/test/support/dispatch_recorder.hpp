/**
 * @file dispatch_recorder.hpp
 * @brief Records endpoint receive callbacks for dispatch unit tests.
 */

#pragma once

#include <cstdint>

#include "core/wirespaces_core.hpp"

#include "support/host_fixture.hpp"
#include "support/packet_builder.hpp"

namespace wirespaces::test::support {

class DispatchRecorder {
public:
    void reset() {
        invocation_count_ = 0U;
        last_packet_ = nullptr;
    }

    void onReceive(const PacketBuffer* packet) {
        ++invocation_count_;
        last_packet_ = packet;
    }

    static void thunk(void* context, const PacketBuffer* packet) {
        static_cast<DispatchRecorder*>(context)->onReceive(packet);
    }

    EndpointReceiver handle() {
        return EndpointReceiver{thunk, this};
    }

    uint32_t invocationCount() const {
        return invocation_count_;
    }

    const PacketBuffer* lastPacket() const {
        return last_packet_;
    }

private:
    uint32_t invocation_count_{0U};
    const PacketBuffer* last_packet_{nullptr};
};

class DispatchTableFixture : public DefaultHostFixture {
protected:
    void SetUp() override {
        DefaultHostFixture::SetUp();
        entry_count_ = 0U;
        dispatch_table_ = DispatchTable{dispatch_entries_, 0U};
        recorder_.reset();
    }

    void registerEndpoint(uint16_t endpoint, EndpointReceiver receiver) {
        dispatch_entries_[entry_count_] = DispatchTableEntry{endpoint, receiver};
        ++entry_count_;
        dispatch_table_ = DispatchTable{dispatch_entries_, entry_count_};
    }

    DispatchResult dispatch(TestPacket& packet) {
        return ws_dispatch_packet(&dispatch_table_, asPacketBuffer(&packet));
    }

    DispatchRecorder recorder_{};
    DispatchTableEntry dispatch_entries_[4]{};
    size_t entry_count_{0U};
    DispatchTable dispatch_table_{dispatch_entries_, 0U};
};

} // namespace wirespaces::test::support
