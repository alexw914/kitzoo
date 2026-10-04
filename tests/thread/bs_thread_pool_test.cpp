// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/bs_thread_pool_test.cpp
// Description: Verifies BS thread pool aliases exposed by the public thread pool header.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/thread_pool.hpp>

#include <gtest/gtest.h>

TEST(BSThreadPool, SubmitsTasksAndReturnsResults) {
    kitzoo::thread::BSThreadPool<> pool{2};

    EXPECT_EQ(pool.submit_task([] { return 42; }).get(), 42);
}

TEST(BSThreadPool, LightAliasSubmitsTasks) {
    kitzoo::thread::BSLightThreadPool pool{2};

    EXPECT_EQ(pool.submit_task([] { return 21 * 2; }).get(), 42);
}
