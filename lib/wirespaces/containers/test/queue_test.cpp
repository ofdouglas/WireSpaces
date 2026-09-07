/**
 * @file queue_test.cpp
 * @brief Fixed-capacity FIFO queue enqueue, dequeue, bounds, and locking.
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <wirespaces/containers/queue.h>

namespace wirespaces::test {
namespace {

using wirespaces::containers::NoLock;
using wirespaces::containers::Queue;

// A new queue reports empty and not full.
TEST(QueueTest, StartsEmpty) {
    Queue<int, 4U> queue{};
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());
}

// Enqueue stores one item and dequeue returns it.
TEST(QueueTest, EnqueueDequeueSingleItem) {
    Queue<int, 4U> queue{};
    ASSERT_TRUE(queue.enqueue(42));
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_FALSE(queue.isFull());

    int value{0};
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(queue.isEmpty());
}

// Items leave the queue in first-in-first-out order.
TEST(QueueTest, PreservesFifoOrder) {
    Queue<int, 4U> queue{};
    ASSERT_TRUE(queue.enqueue(1));
    ASSERT_TRUE(queue.enqueue(2));
    ASSERT_TRUE(queue.enqueue(3));

    int value{0};
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 2);
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(queue.isEmpty());
}

// Enqueue fails once the queue reaches capacity.
TEST(QueueTest, RejectsEnqueueWhenFull) {
    Queue<int, 2U> queue{};
    ASSERT_TRUE(queue.enqueue(1));
    ASSERT_TRUE(queue.enqueue(2));
    EXPECT_TRUE(queue.isFull());
    EXPECT_FALSE(queue.enqueue(3));
}

// Dequeue fails while the queue is empty.
TEST(QueueTest, RejectsDequeueWhenEmpty) {
    Queue<int, 2U> queue{};
    int value{0};
    EXPECT_FALSE(queue.dequeue(value));
}

// A full queue can be drained back to empty.
TEST(QueueTest, FillAndDrain) {
    constexpr std::size_t kCapacity{3U};
    Queue<std::uint16_t, kCapacity> queue{};
    for (std::uint16_t index{0U}; index < kCapacity; ++index) {
        ASSERT_TRUE(queue.enqueue(index));
    }
    EXPECT_TRUE(queue.isFull());

    for (std::uint16_t index{0U}; index < kCapacity; ++index) {
        std::uint16_t value{0U};
        ASSERT_TRUE(queue.dequeue(value));
        EXPECT_EQ(value, index);
    }
    EXPECT_TRUE(queue.isEmpty());
}

// Index wrap-around still preserves FIFO order after partial drain and refill.
TEST(QueueTest, WrapAroundPreservesOrder) {
    Queue<int, 3U> queue{};
    ASSERT_TRUE(queue.enqueue(1));
    ASSERT_TRUE(queue.enqueue(2));
    ASSERT_TRUE(queue.enqueue(3));
    EXPECT_TRUE(queue.isFull());

    int value{0};
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 2);

    ASSERT_TRUE(queue.enqueue(4));
    ASSERT_TRUE(queue.enqueue(5));
    EXPECT_TRUE(queue.isFull());

    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 3);
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 4);
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 5);
    EXPECT_TRUE(queue.isEmpty());
}

struct CountingLock {
    int& lock_count;
    int& unlock_count;

    void lock() noexcept {
        ++lock_count;
    }

    void unlock() noexcept {
        ++unlock_count;
    }
};

// Public operations acquire and release the configured lock type.
TEST(QueueTest, UsesProvidedLock) {
    int lock_count{0};
    int unlock_count{0};
    Queue<int, 2U, CountingLock> queue{CountingLock{lock_count, unlock_count}};

    ASSERT_TRUE(queue.enqueue(1));
    EXPECT_EQ(lock_count, 1);
    EXPECT_EQ(unlock_count, 1);

    int value{0};
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(lock_count, 2);
    EXPECT_EQ(unlock_count, 2);
    EXPECT_EQ(value, 1);
}

// NoLock is the default lock type for single-threaded use.
TEST(QueueTest, DefaultsToNoLock) {
    Queue<int, 2U> queue{};
    ASSERT_TRUE(queue.enqueue(7));

    int value{0};
    ASSERT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 7);
}

} // namespace
} // namespace wirespaces::test
