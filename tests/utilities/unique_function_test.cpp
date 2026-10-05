// ---------------------------------------------------------------------------
// kitzoo/utilities unique_function tests
// ---------------------------------------------------------------------------

#include <kitzoo/utilities/unique_function.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace kitzoo::util;

TEST(UniqueFunctionTest, Empty) {
  unique_function<void()> f;
  EXPECT_FALSE(static_cast<bool>(f));
}

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
  std::string big(64, 'x');
  unique_function<std::size_t()> f = [big] { return big.size(); };
  EXPECT_EQ(f(), 64u);
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

  void operator()() {}
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
