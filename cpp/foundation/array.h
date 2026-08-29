/*
 * @file  array.h
 * @brief Fixed-size embedded C++17 array without a standard-library dependency.
 */

#pragma once

#include <cstddef>

namespace wirespaces::foundation {

/**
 * @brief Own a fixed number of contiguous objects.
 *
 * @tparam T Element type.
 * @tparam N Number of elements.
 */
template <typename T, size_t N>
class Array {
public:
    constexpr T* data() noexcept { return storage_; }
    constexpr const T* data() const noexcept { return storage_; }

    static constexpr size_t size() noexcept { return N; }
    static constexpr bool empty() noexcept { return N == 0U; }

    constexpr T& operator[](size_t index) noexcept { return storage_[index]; }
    constexpr const T& operator[](size_t index) const noexcept {
        return storage_[index];
    }

    constexpr T* begin() noexcept { return storage_; }
    constexpr const T* begin() const noexcept { return storage_; }
    constexpr T* end() noexcept { return storage_ + N; }
    constexpr const T* end() const noexcept { return storage_ + N; }

private:
    T storage_[(N == 0U) ? 1U : N]{};
};

} // namespace wirespaces::foundation
