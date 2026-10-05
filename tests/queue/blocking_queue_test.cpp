// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/queue/blocking_queue_test.cpp
// Description: Verifies blocking queue operations and edge cases.
// -----------------------------------------------------------------------------

#include <kitzoo/queue/blocking_queue.hpp>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::queue;

TEST(BlockingQueueTest, PushAndPop) {
  BlockingQueue<int> q;
  EXPECT_TRUE(q.push(42));
  auto v = q.wait_and_pop();
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 42);
}

TEST(BlockingQueueTest, TryPopEmpty) {
  BlockingQueue<int> q;
  EXPECT_EQ(q.try_pop(), std::nullopt);
}

TEST(BlockingQueueTest, TryPopNonEmpty) {
  BlockingQueue<int> q;
  q.push(1);
  q.push(2);
  EXPECT_EQ(q.try_pop(), 1);
  EXPECT_EQ(q.try_pop(), 2);
  EXPECT_EQ(q.try_pop(), std::nullopt);
}

TEST(BlockingQueueTest, WaitAndPopBlocks) {
  BlockingQueue<int> q;
  std::atomic<bool> pushed{false};

  std::jthread producer{[&] {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    pushed.store(true);
    q.push(7);
  }};

  auto v = q.wait_and_pop();
  ASSERT_TRUE(v.has_value());
  EXPECT_TRUE(pushed.load()); // proves we blocked until the push
  EXPECT_EQ(*v, 7);
}

TEST(BlockingQueueTest, FifoOrder) {
  BlockingQueue<int> q;
  for (int i = 0; i < 100; ++i)
    q.push(i);
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(q.wait_and_pop().value(), i);
}

TEST(BlockingQueueTest, Empty) {
  BlockingQueue<int> q;
  EXPECT_TRUE(q.empty());
  q.push(1);
  EXPECT_FALSE(q.empty());
}

TEST(BlockingQueueTest, Size) {
  BlockingQueue<int> q;
  EXPECT_EQ(q.size(), 0u);
  q.push(1);
  q.push(2);
  EXPECT_EQ(q.size(), 2u);
  static_cast<void>(q.try_pop());
  EXPECT_EQ(q.size(), 1u);
}

TEST(BlockingQueueTest, StringType) {
  BlockingQueue<std::string> q;
  q.push("hello");
  q.push("world");
  EXPECT_EQ(q.wait_and_pop().value(), "hello");
  EXPECT_EQ(q.wait_and_pop().value(), "world");
}

TEST(BlockingQueueTest, MoveOnlyType) {
  BlockingQueue<std::unique_ptr<int>> q;
  q.push(std::make_unique<int>(42));
  auto ptr = q.wait_and_pop();
  ASSERT_TRUE(ptr.has_value());
  EXPECT_EQ(**ptr, 42);
}

TEST(BlockingQueueTest, ConcurrentPushPop) {
  constexpr int kItems = 1000;
  BlockingQueue<int> q;
  std::atomic<int> sum{0};

  std::jthread consumer{[&] {
    for (int i = 0; i < kItems; ++i) {
      sum.fetch_add(q.wait_and_pop().value());
    }
  }};

  std::jthread producer{[&] {
    for (int i = 0; i < kItems; ++i)
      q.push(1);
  }};

  producer.join();
  consumer.join();
  EXPECT_EQ(sum.load(), kItems);
}

// -- close() semantics --------------------------------------------------------

TEST(BlockingQueueTest, CloseWakesConsumers) {
  BlockingQueue<int> q;
  std::atomic<bool> consumer_returned{false};

  std::jthread consumer{[&] {
    auto v = q.wait_and_pop(); // blocks
    EXPECT_EQ(v, std::nullopt);
    consumer_returned.store(true);
  }};

  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  EXPECT_FALSE(consumer_returned.load());
  q.close();
  consumer.join();
  EXPECT_TRUE(consumer_returned.load());
}

TEST(BlockingQueueTest, PushAfterCloseFails) {
  BlockingQueue<int> q;
  q.close();
  EXPECT_FALSE(q.push(1));
  EXPECT_TRUE(q.is_closed());
}

TEST(BlockingQueueTest, CloseDrainsRemainingItems) {
  BlockingQueue<int> q;
  q.push(1);
  q.push(2);
  q.close();

  EXPECT_EQ(q.wait_and_pop().value(), 1);
  EXPECT_EQ(q.wait_and_pop().value(), 2);
  EXPECT_EQ(q.wait_and_pop(), std::nullopt); // drained + closed
}

TEST(BlockingQueueTest, MultipleConsumersOnClose) {
  BlockingQueue<int> q;
  std::atomic<int> wakeups{0};

  auto consumer_fn = [&] {
    while (q.wait_and_pop().has_value()) {
    }
    wakeups.fetch_add(1);
  };

  {
    std::jthread c1{consumer_fn};
    std::jthread c2{consumer_fn};
    std::jthread c3{consumer_fn};
    q.close(); // must wake ALL consumers
  }
  EXPECT_EQ(wakeups.load(), 3);
}

TEST(BlockingQueueTest, BoundedPushBlocksUntilSpace) {
  BlockingQueue<int> q{1};
  EXPECT_EQ(q.capacity(), 1u);
  ASSERT_TRUE(q.push(1));
  std::atomic<bool> pushed{false};
  std::jthread producer{[&] {
    EXPECT_TRUE(q.push(2));
    pushed = true;
  }};
  std::this_thread::sleep_for(std::chrono::milliseconds{50});
  EXPECT_FALSE(pushed.load());
  EXPECT_EQ(q.wait_and_pop().value(), 1);
  producer.join();
  EXPECT_TRUE(pushed.load());
  EXPECT_EQ(q.try_pop().value(), 2);
}

TEST(BlockingQueueTest, CloseWakesBlockedProducer) {
  BlockingQueue<int> q{1};
  ASSERT_TRUE(q.push(1));
  std::jthread producer{[&] { EXPECT_FALSE(q.push(2)); }};
  std::this_thread::sleep_for(std::chrono::milliseconds{20});
  q.close();
  producer.join();
  EXPECT_EQ(q.size(), 1u);
}

TEST(BlockingQueueTest, TryPushKeepsValueWhenFull) {
  BlockingQueue<std::unique_ptr<int>> q{1};
  EXPECT_TRUE(q.try_push(std::make_unique<int>(1)));
  auto value = std::make_unique<int>(2);
  EXPECT_FALSE(q.try_push(std::move(value)));
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 2);
  q.close();
  EXPECT_FALSE(q.try_push(std::make_unique<int>(3)));
}

TEST(BlockingQueueTest, PopForTimesOutAndReturnsItems) {
  BlockingQueue<int> q;
  const auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(q.pop_for(std::chrono::milliseconds{30}), std::nullopt);
  EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds{30});
  std::jthread producer{[&] {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    q.push(7);
  }};
  EXPECT_EQ(q.pop_for(std::chrono::seconds{5}), 7);
}
