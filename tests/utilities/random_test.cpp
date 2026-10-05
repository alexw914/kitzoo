// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/utilities/random_test.cpp
// Description: Verifies random operations and edge cases.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/random.hpp>

#include <algorithm>
#include <cctype>
#include <gtest/gtest.h>
#include <numeric>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace kitzoo::util;

TEST(RandomTest, IntInRange) {
  for (int i = 0; i < 1000; ++i) {
    const auto v = random_int(1, 6);
    EXPECT_GE(v, 1);
    EXPECT_LE(v, 6);
  }
}

TEST(RandomTest, RealInRange) {
  for (int i = 0; i < 1000; ++i) {
    const auto v = random_real(0.0, 1.0);
    EXPECT_GE(v, 0.0);
    EXPECT_LT(v, 1.0);
  }
}

TEST(RandomTest, StringLengthAndCharset) {
  const auto s = random_string(64);
  EXPECT_EQ(s.size(), 64u);
  for (const char c : s) {
    EXPECT_TRUE(std::isalnum(static_cast<unsigned char>(c)));
  }
}

TEST(RandomTest, StringCustomCharset) {
  const auto s = random_string(100, "ab");
  for (const char c : s) {
    EXPECT_TRUE(c == 'a' || c == 'b');
  }
}

TEST(RandomTest, StringsAreDifferent) {
  // Astronomically unlikely to collide for 16-char alnum strings.
  EXPECT_NE(random_string(16), random_string(16));
}

TEST(RandomTest, ShuffleKeepsElements) {
  std::vector<int> v(100);
  std::iota(v.begin(), v.end(), 0);
  const auto original = v;
  shuffle(v);
  std::sort(v.begin(), v.end());
  EXPECT_EQ(v, original);
}

TEST(RandomTest, ThreadLocalEnginesAreIndependent) {
  // Two threads sampling from their own engines should not deadlock
  // and should produce values in range.
  std::vector<std::jthread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([] {
      for (int i = 0; i < 1000; ++i) {
        const auto v = random_int(0, 100);
        EXPECT_GE(v, 0);
        EXPECT_LE(v, 100);
      }
    });
  }
}
