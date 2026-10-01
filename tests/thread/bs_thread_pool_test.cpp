#include <kitzoo/thread/bs_thread_pool.hpp>

#include <gtest/gtest.h>

TEST(BSThreadPool, SubmitsTasksAndReturnsResults) {
    kitzoo::thread::BSThreadPool<> pool{2};

    EXPECT_EQ(pool.submit_task([] { return 42; }).get(), 42);
}

TEST(BSThreadPool, LightAliasSubmitsTasks) {
    kitzoo::thread::BSLightThreadPool pool{2};

    EXPECT_EQ(pool.submit_task([] { return 21 * 2; }).get(), 42);
}
