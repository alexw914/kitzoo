// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/unique_function_test.cpp
// Description: Verifies ownership and invocation of move-only callable wrappers.
// -----------------------------------------------------------------------------

#include <kitzoo/core/unique_function.hpp>

#include <array>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>

using namespace kitzoo::core;

TEST(UniqueFunctionTest, Empty) {
  unique_function<void()> f;
  EXPECT_FALSE(static_cast<bool>(f));
}

#ifndef NDEBUG
TEST(UniqueFunctionTest, EmptyCallAssertsInDebug) {
  unique_function<void()> f;
  EXPECT_DEATH(f(), "empty");
}
#endif

TEST(UniqueFunctionTest, InvokeSmallLambda) {
  unique_function<int(int)> f = [](int x) { return x * 2; };
  EXPECT_EQ(f(21), 42);
}

TEST(UniqueFunctionTest, MoveOnlyCapture) {
  auto ptr = std::make_unique<int>(7);
  unique_function<int()> f = [p = std::move(ptr)] { return *p; };
  EXPECT_EQ(f(), 7);
}

TEST(UniqueFunctionTest, LargeCallableHeapAllocates) {
  // Capture more than the 24-byte SBO buffer.
  std::array<std::size_t, 16> big{};
  big.back() = 64;
  unique_function<std::size_t()> f = [big] { return big.back(); };
  auto moved = std::move(f);
  EXPECT_EQ(moved(), 64u);
}

TEST(UniqueFunctionTest, MoveTransfersOwnership) {
  int count = 0;
  unique_function<void()> a = [&count] { ++count; };
  unique_function<void()> b = std::move(a);
  EXPECT_FALSE(static_cast<bool>(a));
  ASSERT_TRUE(static_cast<bool>(b));
  b();
  EXPECT_EQ(count, 1);
}

namespace {

// Tracks the number of LIVE objects (ctor ++, dtor --), immune to
// temporary/moved-from bookkeeping noise.
struct Watchdog {
  int& alive;

  explicit Watchdog(int& c) : alive{c} { ++alive; }

  Watchdog(Watchdog&& o) noexcept : alive{o.alive} { ++alive; }

  ~Watchdog() { --alive; }

  auto operator()() -> void {}
};

} // namespace

TEST(UniqueFunctionTest, MoveAssignmentDestroysOld) {
  int alive = 0;
  {
    unique_function<void()> a = Watchdog{alive};
    EXPECT_EQ(alive, 1); // one live object, inside a
    unique_function<void()> b = [] {};
    b = std::move(a);    // b's lambda destroyed; watchdog moved into b
    EXPECT_EQ(alive, 1); // still exactly one
    b();
  }
  EXPECT_EQ(alive, 0); // all destroyed
}

TEST(UniqueFunctionTest, DestructorCallsTargetDestructor) {
  int alive = 0;
  {
    unique_function<void()> f = Watchdog{alive};
    EXPECT_EQ(alive, 1);
  }
  EXPECT_EQ(alive, 0);
}

TEST(UniqueFunctionTest, VoidAndNonVoid) {
  unique_function<void(int)> v = [](int) {};
  v(1);
  unique_function<std::string(std::string)> s = [](std::string in) { return in + "!"; };
  EXPECT_EQ(s("hi"), "hi!");
}

TEST(UniqueFunctionTest, ThrowingMoveCallableIsNotMovedWithWrapper) {
  struct ThrowingMove {
    int* moves;

    explicit ThrowingMove(int* counter) : moves(counter) {}

    ThrowingMove(ThrowingMove&& other) noexcept(false) : moves(other.moves) { ++*moves; }

    auto operator()() const -> int { return 7; }
  };

  int moves = 0;
  unique_function<int()> first{ThrowingMove{&moves}};
  moves = 0;
  auto second = std::move(first);
  EXPECT_EQ(moves, 0);
  EXPECT_EQ(second(), 7);
}

TEST(UniqueFunctionTest, VoidSignatureDiscardsResult) {
  int calls = 0;
  unique_function<void()> f = [&calls] { return ++calls; };
  f();
  EXPECT_EQ(calls, 1);
}

TEST(UniqueFunctionTest, NullFunctionAndMemberPointersAreEmpty) {
  struct Counter {
    auto value() const -> int { return 3; }
  };

  int (*function)() = nullptr;
  auto (Counter::*member)() const->int = nullptr;
  EXPECT_FALSE(static_cast<bool>(unique_function<int()>{function}));
  EXPECT_FALSE(static_cast<bool>(unique_function<int(const Counter&)>{member}));

  unique_function<int(const Counter&)> bound{&Counter::value};
  ASSERT_TRUE(static_cast<bool>(bound));
  EXPECT_EQ(bound(Counter{}), 3);
}

TEST(UniqueFunctionTest, RejectsCallablesNotInvocableAsLvalue) {
  struct RvalueOnly {
    auto operator()() && -> int { return 1; }
  };

  EXPECT_FALSE((std::is_constructible_v<unique_function<int()>, RvalueOnly>));
  EXPECT_FALSE((std::is_constructible_v<unique_function<int()>, int>));
}

TEST(UniqueFunctionTest, PackagedTaskSurvivesMoves) {
  std::packaged_task<int()> task{[] { return 5; }};
  auto result = task.get_future();
  unique_function<void()> f = [t = std::move(task)]() mutable { t(); };
  auto moved = std::move(f);
  moved();
  EXPECT_EQ(result.get(), 5);
}
