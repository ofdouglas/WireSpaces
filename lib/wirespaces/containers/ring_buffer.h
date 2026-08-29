/*
 * @file  ring_buffer.h
 * @brief Fixed-capacity single-producer/single-consumer FIFO.
 */

#pragma once

#include <wirespaces/foundation/array.h>
#include <wirespaces/foundation/span.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace wirespaces::containers {

/**
 * @brief FIFO queue that is thread-safe for one producer and one consumer.
 *
 * @tparam T Item type.
 * @tparam kCapacity Number of items the queue can contain.
 * @tparam IndexType Type of the index used to track the write and read positions.
 */
template <typename T, std::size_t kCapacity, typename IndexType = std::uint16_t>
class RingBuffer {
public:
    static_assert(std::is_default_constructible<T>::value, "T must be default constructible");
    static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible");
    static_assert(std::is_copy_assignable<T>::value, "T must be copy assignable");
    static_assert(kCapacity > 0U, "kCapacity must be greater than zero");
    static_assert(std::is_unsigned<IndexType>::value, "IndexType must be an unsigned integer type");

    RingBuffer() = default;
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    bool isEmpty() const noexcept {
        return write_index_ == read_index_;
    }
    bool isFull() const noexcept {
        return increment(write_index_) == read_index_;
    }

    IndexType size() const noexcept {
        const IndexType writer{write_index_.load()};
        const IndexType reader{read_index_.load()};
        if (writer >= reader) {
            return writer - reader;
        }
        return (kCapacity + 1U - reader) + writer;
    }

    bool enqueue(const T& item) noexcept {
        if (isFull()) {
            return false;
        }
        buffer_[write_index_] = item;
        write_index_ = increment(write_index_);
        return true;
    }

    bool enqueue(foundation::Span<const T> items) noexcept {
        for (const auto& item : items) {
            if (!enqueue(item)) {
                return false;
            }
        }
        return true;
    }

    bool dequeue(T& item) noexcept {
        if (isEmpty()) {
            return false;
        }
        item = buffer_[read_index_];
        read_index_ = increment(read_index_);
        return true;
    }

    IndexType dequeue(foundation::Span<T> output) noexcept {
        IndexType index{0U};
        while ((index < output.size()) && dequeue(output[index])) {
            ++index;
        }
        return index;
    }

    bool peek(T& item) const noexcept {
        if (isEmpty()) {
            return false;
        }
        item = buffer_[read_index_];
        return true;
    }

    void clear() noexcept {
        while (!isEmpty()) {
            T item{};
            static_cast<void>(dequeue(item));
        }
    }

private:
    IndexType increment(IndexType index) const noexcept {
        return (index == kCapacity) ? 0U : index + 1U;
    }

    foundation::Array<T, kCapacity + 1U> buffer_{};
    std::atomic<IndexType> write_index_{};
    std::atomic<IndexType> read_index_{};
};

}  // namespace wirespaces::containers
