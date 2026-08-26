#pragma once
/*
 * @file  static_string.h
 * @brief A simple string with internal storage (static / fixed-size array).
 */

#include <array>
#include <cstddef>
#include <cstring>

namespace util {

/** Default capacity for a StaticString. */
constexpr size_t kDefaultStaticStringCapacity = 100U;

/**
 * A simple string with internal storage (static / fixed-size array).
 * @tparam N The capacity of the string.
 */
template <size_t N = kDefaultStaticStringCapacity>
class StaticString {
public:
    StaticString() : data_{}, length_(0U) {
        data_[0] = '\0';
    }

    StaticString(const char* str, size_t input_length) : StaticString() {
        assign(str, input_length);
    }

    explicit StaticString(const char* str) : StaticString(str, bounded_strlen(str)) {}

    template <size_t M>
    explicit StaticString(const StaticString<M>& other) : StaticString() {
        assign(other.data(), other.length());
    }

    void assign(const char* str, size_t input_length) {
        length_ = (input_length > N) ? N : input_length;
        if (length_ > 0U) {
            std::memcpy(data_.data(), str, length_);
        }
        data_[length_] = '\0';
    }

    void append(const char* str, size_t input_length) {
        const size_t available{N - length_};
        const size_t to_copy{(input_length > available) ? available : input_length};
        if (to_copy == 0U) {
            return;
        }
        std::memcpy(data_.data() + length_, str, to_copy);
        length_ += to_copy;
        data_[length_] = '\0';
    }

    void append(const char* str) {
        append(str, bounded_strlen(str));
    }

    void append(char c) {
        if (length_ < N) {
            data_[length_++] = c;
            data_[length_] = '\0';
        }
    }

    void clear() {
        length_ = 0U;
        data_[0] = '\0';
    }

    const char* data() const {
        return data_.data();
    }

    size_t length() const {
        return length_;
    }

    static constexpr size_t capacity() {
        return N;
    }

    char operator[](size_t position) const {
        return data_[position];
    }

    char& operator[](size_t position) {
        return data_[position];
    }

    template <size_t M>
    StaticString& operator=(const StaticString<M>& other) {
        assign(other.data(), other.length());
        return *this;
    }

private:
    static size_t bounded_strlen(const char* str) {
        size_t len{0U};
        while ((len < N) && (str[len] != '\0')) {
            ++len;
        }
        return len;
    }

    std::array<char, N + 1U> data_{};
    size_t length_{0U};
};

inline namespace literals {
inline StaticString<> operator""_l(const char* data, size_t length) {
    return StaticString<>(data, length);
}
} // literals
} // namespace util
