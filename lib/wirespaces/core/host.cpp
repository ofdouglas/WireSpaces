/**
 * @file host.cpp
 * @brief Local WireSpaces host configuration.
 */

#include <wirespaces/core/host.h>

namespace wirespaces {
namespace {
HostInfo g_host_info{};
}  // namespace

bool HostInfo::isMemberOf(WireNumber wire) const noexcept {
    for (uint8_t index = 0U; index < wire_count && index < kMaximumHostWires; ++index) {
        if (wires[index] == wire) {
            return true;
        }
    }
    return false;
}

const HostInfo& localHostInfo() noexcept {
    return g_host_info;
}

void setLocalHostInfo(const HostInfo& host_info) noexcept {
    g_host_info = host_info;
}

}  // namespace wirespaces
