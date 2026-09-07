/*
 * @file  system_monotonic_clock.cpp
 * @brief std::chrono implementation of simulator monotonic time.
 */

#include "sim/monotonic_clock.h"

#include <thread>

namespace wirespaces::sim {

MonotonicClock::TimePoint SystemMonotonicClock::now() const {
    return Clock::now();
}

void SystemMonotonicClock::sleepUntil(const TimePoint deadline) {
    std::this_thread::sleep_until(deadline);
}

}  // namespace wirespaces::sim
