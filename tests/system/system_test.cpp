// ---------------------------------------------------------------------------
// kitzoo/system tests
// ---------------------------------------------------------------------------

#include <kitzoo/system/system.hpp>

#include <cstdlib>
#include <gtest/gtest.h>

using namespace kitzoo::sys;

TEST(SystemTest, GetEnvExisting) {
    // PATH exists on every supported platform
    auto const path = get_env("PATH");
    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->empty());
}

TEST(SystemTest, GetEnvMissing) {
    EXPECT_FALSE(get_env("KITZOO_DEFINITELY_NOT_SET_12345").has_value());
}

TEST(SystemTest, Hostname) {
    auto const hn = hostname();
    EXPECT_FALSE(hn.empty());
}

TEST(SystemTest, CpuCount) {
    EXPECT_GE(cpu_count(), 1u);
}

TEST(SystemTest, Pid) {
    EXPECT_GT(current_pid(), 0L);
    EXPECT_EQ(current_pid(), current_pid());  // stable within process
}

TEST(SystemTest, PageSize) {
    auto const ps = page_size();
    EXPECT_GT(ps, 0u);
    EXPECT_EQ(ps & (ps - 1), 0u);  // power of two
}

TEST(SystemTest, TotalMemory) {
    EXPECT_GT(total_memory(), 0u);
}

TEST(SystemTest, Username) {
    // Environment-dependent: containers often lack $USER AND a utmp entry for
    // getlogin_r. If both are unavailable, username() legitimately returns "".
    auto const name = username();
    if (name.empty()) {
        GTEST_SKIP() << "no USER env and no login session (container environment)";
    }
    EXPECT_FALSE(name.empty());
}

TEST(SystemTest, HomeDir) {
    if (!get_env("HOME").has_value() && !get_env("USERPROFILE").has_value()) {
        GTEST_SKIP() << "no HOME/USERPROFILE in this environment";
    }
    EXPECT_FALSE(home_dir().empty());
}

TEST(SystemTest, StacktraceCapturesFrames) {
    auto const frames = stacktrace();
#if !defined(_WIN32)
    // Symbol names are only resolvable when the binary exports them
    // (-rdynamic); without it we still get return addresses. Only assert
    // that frames were captured at all.
    EXPECT_GT(frames.size(), 1u);
#endif
}