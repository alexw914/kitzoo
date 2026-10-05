// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/thread_pool_test.cpp
// Description: Verifies the built-in thread pool and public BS thread pool aliases.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::thread;

// -- Basic submission & return -------------------------------------------------

TEST(ThreadPoolTest, SubmitSingleReturnsValue) {
  ThreadPool pool{2};
  auto future = pool.submit_task([] { return 42; });
  EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, SubmitWithArguments) {
  ThreadPool pool{2};
  auto future = pool.submit_task([](int a, int b) { return a + b; }, 10, 32);
  EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, SubmitVoid) {
  ThreadPool pool{2};
  std::atomic<int> counter{0};
  auto future = pool.submit_task([&counter] { counter.store(1); });
  future.get();
  EXPECT_EQ(counter.load(), 1);
}

// -- Exception propagation ----------------------------------------------------

TEST(ThreadPoolTest, ExceptionPropagated) {
  ThreadPool pool{2};
  auto future = pool.submit_task([]() -> int { throw std::runtime_error{"bad"}; });
  EXPECT_THROW({ static_cast<void>(future.get()); }, std::runtime_error);
}

// -- Parallel execution -------------------------------------------------------

TEST(ThreadPoolTest, ParallelExecution) {
  constexpr int kTasks = 100;
  ThreadPool pool{4};
  std::vector<std::future<int>> futures;
  futures.reserve(kTasks);

  for (int i = 0; i < kTasks; ++i) {
    futures.push_back(pool.submit_task([i] { return i * i; }));
  }

  for (int i = 0; i < kTasks; ++i) {
    EXPECT_EQ(futures[static_cast<std::size_t>(i)].get(), i * i);
  }
}

// -- Stress test ---------------------------------------------------------------

TEST(ThreadPoolTest, StressTest) {
  constexpr int kTasks = 10'000;
  ThreadPool pool{8};
  std::atomic<int> counter{0};

  std::vector<std::future<void>> futures;
  futures.reserve(kTasks);

  for (int i = 0; i < kTasks; ++i) {
    futures.push_back(pool.submit_task([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
  }

  for (auto& f : futures)
    f.get();
  EXPECT_EQ(counter.load(), kTasks);
}

// -- Shutdown ------------------------------------------------------------------

TEST(ThreadPoolTest, ExplicitShutdown) {
  ThreadPool pool{4};
  auto f = pool.submit_task([] { return 99; });
  pool.shutdown();
  EXPECT_EQ(f.get(), 99);
}

TEST(ThreadPoolTest, TaskSubmissionAfterShutdownThrows) {
  ThreadPool pool{2};
  pool.shutdown();
  EXPECT_THROW(pool.detach_task([] {}), std::runtime_error);
  EXPECT_THROW(pool.submit_task([] {}), std::runtime_error);
}

// -- Thread count -------------------------------------------------------------

TEST(ThreadPoolTest, ThreadCountMatches) {
  ThreadPool pool{4};
  EXPECT_EQ(pool.get_thread_count(), 4u);
}

TEST(ThreadPoolTest, DefaultThreadCount) {
  ThreadPool pool;
  EXPECT_GE(pool.get_thread_count(), 1u);
}

// -- Queued tasks counter -----------------------------------------------------

TEST(ThreadPoolTest, QueuedTasksCount) {
  constexpr int kTasks = 100;
  ThreadPool pool{1}; // single thread to guarantee queuing

  std::vector<std::future<void>> futures;
  for (int i = 0; i < kTasks; ++i) {
    futures.push_back(pool.submit_task([sleep = std::chrono::milliseconds{1}] { std::this_thread::sleep_for(sleep); }));
  }

  const auto queued = pool.get_tasks_queued();
  // Most tasks should still be queued with a single worker
  EXPECT_GT(queued, 0u);

  for (auto& f : futures)
    f.get();
}

// -- Destructor drains tasks ---------------------------------------------------

TEST(ThreadPoolTest, DestructorDrainsTasks) {
  std::atomic<int> counter{0};
  {
    ThreadPool pool{4};
    for (int i = 0; i < 100; ++i) {
      pool.detach_task([&counter] { counter.fetch_add(1); });
    }
  } // destructor waits for all tasks
  EXPECT_EQ(counter.load(), 100);
}

// -- Submission from worker thread --------------------------------------------

TEST(ThreadPoolTest, SubmitFromWorkerThread) {
  ThreadPool pool{4};
  auto future = pool.submit_task([&pool] {
    auto inner = pool.submit_task([] { return 7; });
    return inner.get() * 6;
  });
  EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, DetachTaskRunsAndWaitWaitsForCompletion) {
  ThreadPool pool{2};
  std::atomic<int> counter{0};
  pool.detach_task([&counter] { counter.fetch_add(1); });
  pool.wait();
  EXPECT_EQ(counter.load(), 1);
  EXPECT_EQ(pool.get_tasks_total(), 0u);
}

TEST(ThreadPoolTest, DetachedExceptionDoesNotStopWorker) {
  ThreadPool pool{1};
  pool.detach_task([] { throw std::runtime_error{"detached"}; });
  EXPECT_EQ(pool.submit_task([] { return 42; }).get(), 42);
}

template <typename Pool>
class ThreadPoolBsTest : public ::testing::Test {};

using ThreadPoolBsTypes = ::testing::Types<BSLightThreadPool, BSPriorityThreadPool, BSPauseThreadPool, BSWdcThreadPool>;
TYPED_TEST_SUITE(ThreadPoolBsTest, ThreadPoolBsTypes);

TYPED_TEST(ThreadPoolBsTest, SubmitsTasksAndReturnsResults) {
  TypeParam pool{2};

  EXPECT_EQ(pool.get_thread_count(), 2u);
  EXPECT_EQ(pool.submit_task([]() -> int { return 42; }).get(), 42);
}

TYPED_TEST(ThreadPoolBsTest, PropagatesTaskExceptions) {
  TypeParam pool{1};
  auto future = pool.submit_task([]() -> int { throw std::runtime_error{"bad"}; });

  EXPECT_THROW(static_cast<void>(future.get()), std::runtime_error);
  EXPECT_EQ(pool.submit_task([]() -> int { return 42; }).get(), 42);
}

TYPED_TEST(ThreadPoolBsTest, WaitDrainsDetachedTasks) {
  std::atomic<int> counter{0};
  TypeParam pool{2};

  for (int i = 0; i < 100; ++i) {
    pool.detach_task([&counter]() -> void { counter.fetch_add(1, std::memory_order_relaxed); });
  }
  pool.wait();

  EXPECT_EQ(counter.load(), 100);
  EXPECT_EQ(pool.get_tasks_total(), 0u);
}
