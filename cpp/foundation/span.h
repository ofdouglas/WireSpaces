/*
 * @file  span.h
 * @brief Non-owning contiguous view for embedded C++17.
 */

#pragma once

#include <cstddef>

namespace wirespaces::foundation {

/**
 * @brief View a contiguous sequence without owning it.
 *
 * @tparam T Element type, optionally const-qualified.
 */
template <typename T>
class Span {
public:
    constexpr Span() noexcept = default;
    constexpr Span(T* data, size_t size) noexcept
        : data_{data}
        , size_{size} {}

    template <size_t N>
    constexpr Span(T (&array)[N]) noexcept
        : data_{array}
        , size_{N} {}

    constexpr size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0U; }

    constexpr T& operator[](size_t index) const noexcept { return data_[index]; }
    constexpr T* data() const noexcept { return data_; }
    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end() const noexcept { return data_ + size_; }

    constexpr Span<T> subspan(size_t start, size_t length) const noexcept {
        return Span<T>{data_ + start, length};
    }

    constexpr Span<T> subspan(size_t start) const noexcept {
        return Span<T>{data_ + start, size_ - start};
    }

private:
    T* data_{nullptr};
    size_t size_{0U};
};

} // namespace wirespaces::foundation
