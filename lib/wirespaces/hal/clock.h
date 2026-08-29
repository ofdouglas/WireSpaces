/**
 * @file clock.h
 * @brief WireSpaces monotonic clock HAL.
 */

 #pragma once

 #include <cstdint>
 
 namespace wirespaces::hal {
 
 /**
  * @brief Millisecond-resolution monotonic clock. The clock is started during platform
  * initialization. The details of that are platform-specific, including what reset types
  * (if any) the clock persists through.
  *
  * Intended as the baseline WireSpaces clock for portable timing such as
  * retries, timeouts, and periodic service execution.
  *
  * Time points wrap modulo 2^32. Elapsed-time calculations should use
  * unsigned subtraction so they remain valid across wraparound.
  */
 struct MillisecondClock {
     using TimePoint = std::uint32_t;
     using Duration = std::uint32_t;
 
     /**
      * @brief Get the current monotonic time in milliseconds.
      *
      * @return Current monotonic time in milliseconds, modulo 2^32.
      */
     static TimePoint now() noexcept;
 };
 
 /**
  * @brief Nanosecond-resolution monotonic clock. The clock is started during platform
  * initialization. The details of that are platform-specific, including what reset types
  * (if any) the clock persists through.
  *
  * Intended for components requiring high-resolution timestamps or timing.
  * Portable WireSpaces components should prefer MillisecondClock unless
  * nanosecond resolution is specifically required.
  */
 struct NanosecondClock {
     using TimePoint = std::uint64_t;
     using Duration = std::uint64_t;
 
     /**
      * @brief Get the current monotonic time in nanoseconds.
      *
      * @return Current monotonic time in nanoseconds, modulo 2^64.
      */
     static TimePoint now() noexcept;
 };
 
 }  // namespace wirespaces::hal