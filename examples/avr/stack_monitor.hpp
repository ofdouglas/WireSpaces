/*
 * @file  stack_monitor.hpp
 * @brief Paint-and-scan peak stack monitor for the ATmega328P example.
 */

#pragma once

#include <avr/io.h>

#include <cstdint>

extern "C" std::uint8_t __heap_start;

namespace wirespaces::avr_example {

/**
 * @brief Track peak downward-growing AVR stack consumption.
 *
 * The monitor paints unused SRAM between the linker-provided heap start and
 * the current stack pointer. A guard below the active initialization frame is
 * deliberately left unpainted and therefore counted as baseline usage.
 */
class StackMonitor {
public:
    /**
     * @brief Paint currently unused stack space and establish the baseline.
     *
     * Call once near the beginning of main(), before interrupts are enabled.
     */
    void initialize() noexcept;

    /**
     * @brief Scan the paint boundary and update peak utilization.
     */
    void scan() noexcept;

    /**
     * @brief Return maximum observed stack use in bytes.
     */
    std::uint16_t peakUsedBytes() const noexcept {
        return peak_used_bytes_;
    }

    /**
     * @brief Return SRAM bytes between static allocation and RAM end.
     */
    std::uint16_t capacityBytes() const noexcept {
        return capacity_bytes_;
    }

private:
    static constexpr std::uint8_t kPaintPattern{0xA5U};
    static constexpr std::uint16_t kInitializationGuardBytes{32U};

    std::uint16_t stack_region_start_{0U};
    std::uint16_t capacity_bytes_{0U};
    std::uint16_t peak_used_bytes_{0U};
};

inline void StackMonitor::initialize() noexcept {
    stack_region_start_ = static_cast<std::uint16_t>(
        reinterpret_cast<std::uintptr_t>(&__heap_start));
    const std::uint16_t stack_region_end{
        static_cast<std::uint16_t>(RAMEND + 1U)};
    capacity_bytes_ = static_cast<std::uint16_t>(
        stack_region_end - stack_region_start_);

    const std::uint16_t stack_pointer{SP};
    const std::uint16_t paint_end{
        (stack_pointer > (stack_region_start_ + kInitializationGuardBytes))
            ? static_cast<std::uint16_t>(
                  stack_pointer - kInitializationGuardBytes)
            : stack_region_start_};

    auto* cursor{reinterpret_cast<volatile std::uint8_t*>(
        static_cast<std::uintptr_t>(stack_region_start_))};
    auto* const end{reinterpret_cast<volatile std::uint8_t*>(
        static_cast<std::uintptr_t>(paint_end))};
    while (cursor < end) {
        *cursor++ = kPaintPattern;
    }

    scan();
}

inline void StackMonitor::scan() noexcept {
    const std::uint16_t stack_region_end{
        static_cast<std::uint16_t>(RAMEND + 1U)};
    auto* cursor{reinterpret_cast<volatile const std::uint8_t*>(
        static_cast<std::uintptr_t>(stack_region_start_))};
    auto* const end{reinterpret_cast<volatile const std::uint8_t*>(
        static_cast<std::uintptr_t>(stack_region_end))};
    while ((cursor < end) && (*cursor == kPaintPattern)) {
        ++cursor;
    }

    const auto first_used_address{static_cast<std::uint16_t>(
        reinterpret_cast<std::uintptr_t>(cursor))};
    const std::uint16_t used_bytes{
        static_cast<std::uint16_t>(stack_region_end - first_used_address)};
    if (used_bytes > peak_used_bytes_) {
        peak_used_bytes_ = used_bytes;
    }
}

} // namespace wirespaces::avr_example
