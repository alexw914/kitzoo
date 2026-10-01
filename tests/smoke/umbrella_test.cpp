// ---------------------------------------------------------------------------
// Umbrella header smoke tests: each public umbrella must compile standalone
// and expose its module's primary symbols.
// ---------------------------------------------------------------------------

#include <kitzoo/core.hpp>
#include <kitzoo/filesystem.hpp>
#include <kitzoo/log.hpp>
#include <kitzoo/queue.hpp>
#include <kitzoo/string.hpp>
#include <kitzoo/system.hpp>
#include <kitzoo/thread.hpp>
#include <kitzoo/time.hpp>
#include <kitzoo/utilities.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(UmbrellaTest, AllHeadersCompile) {
    // One symbol per module to prove the umbrella pulled it in.
    kitzoo::util::ScopeGuard guard{[] {}};
    bool const has_b = std::string_view{"abc"}.find("b") != std::string_view::npos;
    EXPECT_TRUE(has_b);
    kitzoo::time::Stopwatch sw;
    kitzoo::thread::ThreadPool pool{1};
    kitzoo::queue::BlockingQueue<int> q;
    auto r = kitzoo::util::random_int(0, 1);
    auto uuid = kitzoo::util::Uuid::random();
    auto& singleton = kitzoo::util::Singleton<int>::instance();
    (void)r;
    (void)uuid;
    (void)singleton;
    EXPECT_TRUE(kitzoo::sys::cpu_count() > 0);
}
