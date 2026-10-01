#include <kitzoo/lock/synchronized.hpp>

#include <gtest/gtest.h>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

TEST(SynchronizedTest, WithLockAndCopy) {
    kitzoo::lock::Synchronized<std::string> value{std::string{"hello"}};
    value.with_lock([](auto& text) { text += " world"; });
    EXPECT_EQ(value.copy(), "hello world");
}

TEST(SynchronizedTest, ConcurrentIncrements) {
    kitzoo::lock::Synchronized<int> counter{0};
    std::vector<std::jthread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&counter] {
            for (int j = 0; j < 1000; ++j)
                counter.with_lock([](int& value) { ++value; });
        });
    }
    threads.clear();
    EXPECT_EQ(counter.copy(), 8000);
}

TEST(SynchronizedTest, SharedMutexRead) {
    kitzoo::lock::Synchronized<int, std::shared_mutex> value{42};
    EXPECT_EQ(value.read([](int const& number) { return number * 2; }), 84);
}
