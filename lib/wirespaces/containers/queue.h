/*
 * @file  queue.h
 * @brief Generic queue implementation.
 */

#pragma once

#include <wirespaces/foundation/array.h>
#include <wirespaces/foundation/span.h>

#include <cstddef>
#include <cstdint>


namespace wirespaces::containers {

struct NoLock {
    void lock() noexcept {}
    void unlock() noexcept {}
};

template <typename T, size_t kCapacity, typename LockType = NoLock>
class Queue {
public:
    explicit Queue(LockType lock = LockType{}) noexcept
        : lock_{static_cast<LockType&&>(lock)} {}

    bool enqueue(const T& item) noexcept {
        lock_.lock();
        bool result = enqueueImpl(item);
        lock_.unlock();
        return result;
    }

    bool dequeue(T& item) noexcept {
        lock_.lock();
        bool result = dequeueImpl(item);
        lock_.unlock();
        return result;
    }

    bool isFull() const noexcept {
        return size_ == kCapacity;
    }

    bool isEmpty() const noexcept {
        return size_ == 0U;
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(Queue&&) = delete;

private:
    uint16_t increment(uint16_t index) const noexcept {
        const uint16_t result = index + 1U;
        return (result >= kCapacity) ? 0U : result;
    }

    bool enqueueImpl(const T& item) noexcept {
        if (isFull()) {
            return false;
        }
        items_[write_index_] = item;
        write_index_ = increment(write_index_);
        size_++;
        return true;
    }

    bool dequeueImpl(T& item) noexcept {
        if (isEmpty()) {
            return false;
        }
        item = items_[read_index_];
        read_index_ = increment(read_index_);
        size_--;
        return true;
    }

    wirespaces::foundation::Array<T, kCapacity> items_{};
    uint16_t read_index_{0U};
    uint16_t write_index_{0U};
    uint16_t size_{0U};
    LockType lock_;
};

}  // namespace wirespaces::containers
