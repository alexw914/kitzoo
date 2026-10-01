#include <kitzoo/lock/spinlock.hpp>

#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

TEST(SpinLockTest, ProtectsSharedState) {
    kitzoo::lock::SpinLock lock;
    int value = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < 10000; ++j) {
                std::lock_guard guard{lock};
                ++value;
            }
        });
    }
    for (auto& thread : threads)
        thread.join();
    EXPECT_EQ(value, 40000);
}
