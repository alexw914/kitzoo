// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/queue/mpmc_queue_test.cpp
// Description: Verifies mpmc queue ordering, bounds, ownership and concurrent delivery.
// -----------------------------------------------------------------------------

#include <kitzoo/queue/mpmc_queue.hpp>

#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace kitzoo::queue;

TEST(MPMCQueueTest, FifoOrderAndEmpty) {
  MPMCQueue<int> q{8};
  EXPECT_TRUE(q.empty());
  for (int i = 0; i < 8; ++i)
    EXPECT_TRUE(q.push(i));
  EXPECT_EQ(q.size(), 8u);
  for (int i = 0; i < 8; ++i)
    EXPECT_EQ(q.pop(), i);
  EXPECT_EQ(q.pop(), std::nullopt);
  EXPECT_TRUE(q.empty());
}

TEST(MPMCQueueTest, CapacityRoundsUpToPowerOfTwoAndAtLeastTwo) {
  EXPECT_EQ(MPMCQueue<int>{1}.capacity(), 2u);
  EXPECT_EQ(MPMCQueue<int>{3}.capacity(), 4u);
  EXPECT_EQ(MPMCQueue<int>{64}.capacity(), 64u);
  EXPECT_THROW(MPMCQueue<int>{0}, std::invalid_argument);
}

TEST(MPMCQueueTest, FullPushFailsWithoutConsumingValueAndWrapsAround) {
  MPMCQueue<std::unique_ptr<int>> q{2};
  for (int round = 0; round < 3; ++round) {
    EXPECT_TRUE(q.push(std::make_unique<int>(1)));
    EXPECT_TRUE(q.push(std::make_unique<int>(2)));
    auto extra = std::make_unique<int>(3);
    EXPECT_FALSE(q.push(std::move(extra)));
    ASSERT_NE(extra, nullptr);
    EXPECT_EQ(**q.pop(), 1);
    EXPECT_EQ(**q.pop(), 2);
  }
}

TEST(MPMCQueueTest, DestructorDestroysRemainingElements) {
  auto tracker = std::make_shared<int>(0);
  {
    MPMCQueue<std::shared_ptr<int>> q{4};
    EXPECT_TRUE(q.push(tracker));
    EXPECT_TRUE(q.push(tracker));
    EXPECT_EQ(tracker.use_count(), 3);
  }
  EXPECT_EQ(tracker.use_count(), 1);
}

TEST(MPMCQueueTest, StringType) {
  MPMCQueue<std::string> q{4};
  EXPECT_TRUE(q.push(std::string{"hello"}));
  EXPECT_EQ(q.pop().value(), "hello");
}

// Each value is delivered exactly once and values from one producer keep their order.
TEST(MPMCQueueTest, ConcurrentProducersAndConsumersDeliverEveryValueInProducerOrder) {
  constexpr int kProducers = 4;
  constexpr int kConsumers = 4;
  constexpr std::int64_t kPerProducer = 100'000;
  MPMCQueue<std::int64_t> q{64};
  std::vector<std::atomic<int>> seen(kProducers * kPerProducer);
  std::atomic<bool> order_ok{true};
  std::atomic<std::int64_t> consumed{0};

  {
    std::vector<std::jthread> threads;
    for (int p = 0; p < kProducers; ++p)
      threads.emplace_back([&, p] {
        for (std::int64_t i = 0; i < kPerProducer; ++i)
          while (!q.push(p * kPerProducer + i)) {
          }
      });
    for (int c = 0; c < kConsumers; ++c)
      threads.emplace_back([&] {
        std::vector<std::int64_t> last(kProducers, -1);
        while (consumed.load(std::memory_order_relaxed) < kProducers * kPerProducer) {
          auto value = q.pop();
          if (!value)
            continue;
          consumed.fetch_add(1, std::memory_order_relaxed);
          seen[static_cast<std::size_t>(*value)].fetch_add(1, std::memory_order_relaxed);
          const auto producer = *value / kPerProducer;
          if (*value <= last[static_cast<std::size_t>(producer)])
            order_ok = false;
          last[static_cast<std::size_t>(producer)] = *value;
        }
      });
  }

  EXPECT_TRUE(order_ok.load());
  EXPECT_TRUE(q.empty());
  for (const auto& count : seen)
    ASSERT_EQ(count.load(), 1);
}
