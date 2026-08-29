/**
 * @file host.h
 * @brief Configuration for the local WireSpaces host.
 */

#pragma once

#include <wirespaces/core/header.h>

#include <cstdint>

namespace wirespaces {

constexpr uint8_t kMaximumHostWires{6U};

struct HostInfo {
    HostId id{};
    uint8_t wire_count{0U};
    WireNumber wires[kMaximumHostWires]{};

    [[nodiscard]] bool isMemberOf(WireNumber wire) const noexcept;
};

[[nodiscard]] const HostInfo& localHostInfo() noexcept;
void setLocalHostInfo(const HostInfo& host_info) noexcept;

}  // namespace wirespaces
