// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/scopeguard_test.cpp
// Description: Verifies ScopeGuard and the KZ_SCOPE_EXIT/FAIL/SUCCESS macros.
// -----------------------------------------------------------------------------

#include <kitzoo/core/scopeguard.hpp>

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

using kitzoo::core::ScopeGuard;

namespace {

struct Counts {
  int exit = 0;
  int fail = 0;
  int success = 0;
};

auto guarded_scope(Counts& counts, bool fail) -> void {
  KZ_SCOPE_EXIT {
    ++counts.exit;
  };
  KZ_SCOPE_FAIL {
    ++counts.fail;
  };
  KZ_SCOPE_SUCCESS {
    ++counts.success;
  };
  if (fail)
    throw std::runtime_error{"failure"};
}

} // namespace

TEST(ScopeGuardTest, NormalExitRunsExitAndSuccessGuards) {
  Counts counts;
  guarded_scope(counts, false);
  EXPECT_EQ(counts.exit, 1);
  EXPECT_EQ(counts.fail, 0);
  EXPECT_EQ(counts.success, 1);
}

TEST(ScopeGuardTest, ExceptionRunsExitAndFailGuards) {
  Counts counts;
  EXPECT_THROW(guarded_scope(counts, true), std::runtime_error);
  EXPECT_EQ(counts.exit, 1);
  EXPECT_EQ(counts.fail, 1);
  EXPECT_EQ(counts.success, 0);
}

TEST(ScopeGuardTest, GuardsRunInReverseDeclarationOrder) {
  std::string order;
  {
    KZ_SCOPE_EXIT {
      order += 'a';
    };
    KZ_SCOPE_EXIT {
      order += 'b';
    };
  }
  EXPECT_EQ(order, "ba");
}

TEST(ScopeGuardTest, DismissDisarmsNamedGuard) {
  int calls = 0;
  {
    ScopeGuard guard{[&]() noexcept { ++calls; }};
    guard.dismiss();
  }
  EXPECT_EQ(calls, 0);
}

// A guard created while another exception unwinds reacts only to a new exception.
TEST(ScopeGuardTest, FailGuardIgnoresExceptionAlreadyInFlight) {
  Counts counts;

  struct Unwinder {
    Counts* counts;

    ~Unwinder() {
      KZ_SCOPE_FAIL {
        ++counts->fail;
      };
      KZ_SCOPE_SUCCESS {
        ++counts->success;
      };
    }
  };

  EXPECT_THROW(
      {
        Unwinder unwinder{&counts};
        throw std::runtime_error{"outer"};
      },
      std::runtime_error);
  EXPECT_EQ(counts.fail, 0);
  EXPECT_EQ(counts.success, 1);
}
