// ---------------------------------------------------------------------------
// kitzoo/queue SPSCQueue tests
//
// TSan is essential here: run with --preset tsan to verify lock-free correctness.
// ---------------------------------------------------------------------------

#include <kitzoo/queue/spsc_queue.hpp>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::queue;

constexpr std::size_t kQueueCap = 1024;

// -- Single-threaded correctness ----------------------------------------------

TEST(SPSCQueueTest, PushPopSingleThread) {
    SPSCQueue<int, kQueueCap> q;
    EXPECT_TRUE(q.empty());

    EXPECT_TRUE(q.push(42));
    EXPECT_FALSE(q.empty());

    auto val = q.pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueueTest, FifoOrder) {
    SPSCQueue<int, kQueueCap> q;
    constexpr int N = 100;
    for (int i = 0; i < N; ++i)
        EXPECT_TRUE(q.push(i));
    for (int i = 0; i < N; ++i) {
        auto v = q.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
}

TEST(SPSCQueueTest, PopEmpty) {
    SPSCQueue<int, kQueueCap> q;
    EXPECT_EQ(q.pop(), std::nullopt);
}

TEST(SPSCQueueTest, PushFull) {
    SPSCQueue<int, 4> q;
    for (int i = 0; i < 4; ++i)
        EXPECT_TRUE(q.push(i));
    EXPECT_FALSE(q.push(99));  // full
}

TEST(SPSCQueueTest, Capacity) {
    SPSCQueue<int, 4> q;
    EXPECT_EQ(q.capacity(), 4u);
}

TEST(SPSCQueueTest, WrapAround) {
    SPSCQueue<int, 4> q;
    // Fill and drain multiple times to exercise wrap-around
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 4; ++i)
            EXPECT_TRUE(q.push(i));
        for (int i = 0; i < 4; ++i) {
            auto v = q.pop();
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(*v, i);
        }
    }
}

TEST(SPSCQueueTest, StringType) {
    SPSCQueue<std::string, 8> q;
    EXPECT_TRUE(q.push("hello"));
    EXPECT_TRUE(q.push("world"));
    EXPECT_EQ(q.pop().value(), "hello");
    EXPECT_EQ(q.pop().value(), "world");
}

TEST(SPSCQueueTest, MoveOnlyType) {
    SPSCQueue<std::unique_ptr<int>, 8> q;
    EXPECT_TRUE(q.push(std::make_unique<int>(42)));
    auto ptr = q.pop().value();
    EXPECT_EQ(*ptr, 42);
}

// -- Multi-threaded stress test (targets TSan verification) -------------------

TEST(SPSCQueueTest, StressSingleProducerSingleConsumer) {
    constexpr int kTotal = 1'000'000;
    SPSCQueue<int, kQueueCap> q;

    std::atomic<bool> start{false};
    std::atomic<int> produced{0};
    std::atomic<int> consumed_sum{0};
    int expected_sum = 0;

    std::jthread producer{[&] {
        while (!start.load()) {
        }
        for (int i = 0; i < kTotal; ++i) {
            while (!q.push(i)) {
            }
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    }};

    std::jthread consumer{[&] {
        while (!start.load()) {
        }
        int received = 0;
        while (received < kTotal) {
            auto v = q.pop();
            if (v.has_value()) {
                consumed_sum.fetch_add(*v, std::memory_order_relaxed);
                ++received;
            }
        }
    }};

    start.store(true);
    producer.join();
    consumer.join();

    EXPECT_EQ(produced.load(), kTotal);
    // Sum of 0..kTotal-1: use int64_t to avoid overflow
    auto const n = static_cast<std::int64_t>(kTotal);
    expected_sum = static_cast<int>((n - 1) * n / 2);
    EXPECT_EQ(consumed_sum.load(), expected_sum);
}

TEST(SPSCQueueTest, ManyRoundTripsNoDataLoss) {
    SPSCQueue<int, kQueueCap> q;
    std::atomic<bool> done{false};
    std::atomic<int> checksum{0};

    std::jthread producer{[&] {
        for (int i = 0; i < 500'000; ++i) {
            while (!q.push(i)) {
            }
        }
        done.store(true, std::memory_order_release);
    }};

    std::jthread consumer{[&] {
        int expected = 0;
        while (!done.load(std::memory_order_acquire) || !q.empty()) {
            auto v = q.pop();
            if (v.has_value()) {
                EXPECT_EQ(*v, expected);
                ++expected;
                checksum.fetch_add(1, std::memory_order_relaxed);
            }
        }
        EXPECT_EQ(expected, 500'000);
    }};

    producer.join();
    consumer.join();
    EXPECT_EQ(checksum.load(), 500'000);
}
