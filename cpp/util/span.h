#pragma once
/*
 * @file  span.h
 * @brief A simple span class for C++17 and later.
 */

#include <array>
#include <cstddef>

namespace util {

template <typename T>
class Span {
public:
    /******************************************************************************
     *  Constructors
     ******************************************************************************/
    Span() = default;
    Span(T* data, size_t size) : data_(data), size_(size) {}

    template <size_t N>
    Span(T (&array)[N]) : data_(array), size_(N) {}

    template <size_t N>
    Span(std::array<T, N>& array) : data_(array.data()), size_(N) {}

    template <size_t N>
    Span(const std::array<T, N>& array) : data_(array.data()), size_(N) {}

    /******************************************************************************
     *  Size Getters
     ******************************************************************************/
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0U; }

    /******************************************************************************
     *  Data Accessors
     ******************************************************************************/
    const T& operator[](size_t i) const {
        return data_[i];
    }
    T& operator[](size_t i) {
        return data_[i];
    }

    const T* data() const { return data_; }
    T* data() { return data_; }

    const T* begin() const { return data_; }
    T* begin() { return data_; }

    const T* end() const { return data_ + size_; }
    T* end() { return data_ + size_; }

    const util::Span<T> subspan(size_t start, size_t length) const {
        return util::Span<T>(data_ + start, length);
    }
    util::Span<T> subspan(size_t start, size_t length) {
        return util::Span<T>(data_ + start, length);
    }

    const util::Span<T> subspan(size_t start) const {
        return util::Span<T>(data_ + start, size_ - start);
    }
    util::Span<T> subspan(size_t start) {
        return util::Span<T>(data_ + start, size_ - start);
    }

private:
    T* data_{};
    size_t size_{0U};
};

} // namespace util
