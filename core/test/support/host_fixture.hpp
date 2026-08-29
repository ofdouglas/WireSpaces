/**
 * @file host_fixture.hpp
 * @brief Configures local host membership for core tests.
 */

#pragma once

#include <gtest/gtest.h>

#include <runtime/core.hpp>

#include "support/constants.hpp"

namespace wirespaces::test::support {

inline void configureHost(HostId host_id, WireNumber wire) {
    HostInfo host_info{};
    host_info.id = host_id;
    host_info.wire_count = 1U;
    host_info.wires[0] = wire;
    setLocalHostInfo(host_info);
}

inline void configureDefaultHost() { configureHost(kLocalHostId, kLocalWire); }
inline void configureHostWithoutLocalWire() { configureHost(kLocalHostId, kOtherWire); }

class DefaultHostFixture : public ::testing::Test {
protected:
    void SetUp() override { configureDefaultHost(); }
};

} // namespace wirespaces::test::support
