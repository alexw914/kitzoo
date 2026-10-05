// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/scope_guard_test.cpp
// Description: Verifies scope guard cleanup, dismissal and move semantics.
// -----------------------------------------------------------------------------

#include <kitzoo/core/scope_guard.hpp>

#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>

using kitzoo::core::ScopeGuard;

TEST(ScopeGuardTest, RunsCleanupOnScopeExit) {
  int calls = 0;
  {
    ScopeGuard guard{[&] { ++calls; }};
    EXPECT_EQ(calls, 0);
  }
  EXPECT_EQ(calls, 1);
}

TEST(ScopeGuardTest, RunsCleanupWhenExceptionUnwinds) {
  int calls = 0;
  EXPECT_THROW(
      {
        ScopeGuard guard{[&] { ++calls; }};
        throw std::runtime_error{"failure"};
      },
      std::runtime_error);
  EXPECT_EQ(calls, 1);
}

TEST(ScopeGuardTest, DismissSkipsCleanup) {
  int calls = 0;
  {
    ScopeGuard guard{[&] { ++calls; }};
    guard.dismiss();
  }
  EXPECT_EQ(calls, 0);
}

TEST(ScopeGuardTest, MovedGuardRunsCleanupOnce) {
  int calls = 0;
  {
    ScopeGuard first{[&] { ++calls; }};
    ScopeGuard second{std::move(first)};
  }
  EXPECT_EQ(calls, 1);
}
