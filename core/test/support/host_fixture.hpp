/**
 * @file host_fixture.hpp
 * @brief Configures HostInfo for tests that depend on local host membership.
 */

#pragma once

#include <gtest/gtest.h>

#include <runtime/core.hpp>

#include "support/constants.hpp"

namespace wirespaces::test::support {

inline void configureHost(uint8_t host_id, uint8_t num_wires, const uint8_t* wires) {
    HostInfo host_info{};
    host_info.host_id = host_id;
    host_info.num_wires = num_wires;
    for (uint8_t index = 0U; index < num_wires; ++index) {
        host_info.wires[index] = wires[index];
    }
    ws_host_set_info(&host_info);
}

inline void configureDefaultHost() {
    const uint8_t wires[] = {WS_WIRE_LOCAL_DOMAIN};
    configureHost(kLocalHostId, 1U, wires);
}

inline void configureHostWithoutLocalWire() {
    const uint8_t wires[] = {kOtherWire};
    configureHost(kLocalHostId, 1U, wires);
}

class DefaultHostFixture : public ::testing::Test {
protected:
    void SetUp() override {
        configureDefaultHost();
    }
};

} // namespace wirespaces::test::support
