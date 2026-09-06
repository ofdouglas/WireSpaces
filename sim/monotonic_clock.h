#pragma once
/*
 * @file  monotonic_clock.h
 * @brief Injectable monotonic time and sleep contracts for the ECU superloop.
 */

#include <chrono>

namespace wirespaces::sim {

/**
 * @brief Monotonic clock seam used to make superloop timing deterministic.
 *
 * Implementations must never move backward. sleepUntil() may return late but
 * must not intentionally return before the supplied deadline.
 */
class MonotonicClock {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    virtual ~MonotonicClock() = default;

    /**
     * @brief Read the current monotonic time.
     */
    virtual TimePoint now() const = 0;

    /**
     * @brief Suspend execution until at least the supplied monotonic deadline.
     */
    virtual void sleepUntil(TimePoint deadline) = 0;
};

/**
 * @brief Production monotonic clock backed by std::chrono::steady_clock.
 */
class SystemMonotonicClock final : public MonotonicClock {
public:
    TimePoint now() const override;
    void sleepUntil(TimePoint deadline) override;
};

}  // namespace wirespaces::sim
