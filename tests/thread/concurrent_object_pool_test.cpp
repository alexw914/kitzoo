#include <kitzoo/thread/concurrent_object_pool.hpp>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using kitzoo::thread::ConcurrentObjectPool;

TEST(ConcurrentObjectPoolTest, LeaseReturnsObjectToPool) {
    ConcurrentObjectPool<std::string> pool;
    pool.add(std::make_unique<std::string>("reused"));

    {
        auto object = pool.acquire();
        EXPECT_EQ(*object, "reused");
        EXPECT_EQ(pool.available_approx(), 0u);
    }

    EXPECT_EQ(pool.available_approx(), 1u);
}

TEST(ConcurrentObjectPoolTest, SupportsNonblockingAndTimedAcquire) {
    ConcurrentObjectPool<int> pool;
    EXPECT_FALSE(pool.try_acquire());
    EXPECT_FALSE(pool.acquire_for(1ms));

    pool.add(std::make_unique<int>(42));
    auto object = pool.acquire_for(10ms);
    ASSERT_TRUE(object);
    EXPECT_EQ(**object, 42);
}

TEST(ConcurrentObjectPoolTest, LeaseCanReturnFromAnotherThread) {
    ConcurrentObjectPool<int> pool;
    pool.add(std::make_unique<int>(7));
    auto object = pool.acquire();

    std::thread worker{[lease = std::move(object)]() mutable { lease.reset(); }};
    worker.join();

    EXPECT_EQ(pool.available_approx(), 1u);
    EXPECT_EQ(*pool.acquire(), 7);
}

TEST(ConcurrentObjectPoolTest, SupportsConcurrentAcquireAndReturn) {
    struct Counter {
        std::atomic<int> uses{0};
    };

    ConcurrentObjectPool<Counter> pool;
    constexpr int object_count = 4;
    constexpr int thread_count = 8;
    constexpr int iterations = 200;
    for (int i = 0; i < object_count; ++i)
        pool.add(std::make_unique<Counter>());

    std::vector<std::thread> workers;
    for (int i = 0; i < thread_count; ++i) {
        workers.emplace_back([&pool] {
            for (int j = 0; j < iterations; ++j) {
                auto object = pool.acquire();
                object->uses.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& worker : workers)
        worker.join();

    int total = 0;
    std::vector<ConcurrentObjectPool<Counter>::UniquePtr> objects;
    for (int i = 0; i < object_count; ++i) {
        objects.push_back(pool.acquire());
        total += objects.back()->uses.load(std::memory_order_relaxed);
    }
    EXPECT_EQ(total, thread_count * iterations);
    EXPECT_EQ(pool.available_approx(), 0u);
    objects.clear();
    EXPECT_EQ(pool.available_approx(), static_cast<std::size_t>(object_count));
}

TEST(ConcurrentObjectPoolTest, SharedLeaseReturnsAfterLastOwner) {
    ConcurrentObjectPool<int> pool;
    pool.add(std::make_unique<int>(9));

    auto first = pool.acquire_shared();
    auto second = first;
    first.reset();
    EXPECT_EQ(pool.available_approx(), 0u);

    second.reset();
    EXPECT_EQ(pool.available_approx(), 1u);
}

TEST(ConcurrentObjectPoolTest, RejectsNullObjects) {
    ConcurrentObjectPool<int> pool;
    EXPECT_THROW(pool.add(nullptr), std::invalid_argument);
}
