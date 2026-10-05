// ---------------------------------------------------------------------------
// kitzoo/string unit tests
// ---------------------------------------------------------------------------

#include <kitzoo/string/string_utils.hpp>

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

using namespace kitzoo::str;

// ============================================================================
// trim
// ============================================================================

TEST(TrimTest, Empty) {
  EXPECT_EQ(trim(""), "");
  EXPECT_EQ(trim_left(""), "");
  EXPECT_EQ(trim_right(""), "");
}

TEST(TrimTest, WhitespaceOnly) {
  EXPECT_EQ(trim("   "), "");
  EXPECT_EQ(trim("\t\n\r\f\v"), "");
}

TEST(TrimTest, NoWhitespace) {
  EXPECT_EQ(trim("hello"), "hello");
  EXPECT_EQ(trim_left("hello"), "hello");
  EXPECT_EQ(trim_right("hello"), "hello");
}

TEST(TrimTest, LeadingOnly) {
  EXPECT_EQ(trim("  hello"), "hello");
  EXPECT_EQ(trim_left("  hello"), "hello");
  EXPECT_EQ(trim_right("  hello"), "  hello");
}

TEST(TrimTest, TrailingOnly) {
  EXPECT_EQ(trim("hello  "), "hello");
  EXPECT_EQ(trim_left("hello  "), "hello  ");
  EXPECT_EQ(trim_right("hello  "), "hello");
}

TEST(TrimTest, BothSides) {
  EXPECT_EQ(trim("  hello  "), "hello");
}

TEST(TrimTest, MixedWhitespace) {
  EXPECT_EQ(trim("\t\n hello \r\f"), "hello");
}

TEST(TrimTest, SingleCharacter) {
  EXPECT_EQ(trim("x"), "x");
  EXPECT_EQ(trim_left(" "), "");
  EXPECT_EQ(trim_right(" "), "");
}

TEST(TrimTest, ViewOfOriginal) {
  std::string const original = "  hello  ";
  auto const view = trim(original);
  // view should point within original's buffer
  EXPECT_GE(view.data(), original.data());
  EXPECT_LT(view.data(), original.data() + original.size());
}

TEST(TrimTest, ConstexprWorks) {
  constexpr auto v = trim("  hi  ");
  static_assert(v == "hi");
}

// ============================================================================
// join
// ============================================================================

TEST(JoinTest, EmptySpan) {
  auto const result = join(std::span<std::string_view const>{}, ",");
  EXPECT_TRUE(result.empty());
}

TEST(JoinTest, SingleElement) {
  std::vector<std::string_view> const parts{"hello"};
  EXPECT_EQ(join(parts, ","), "hello");
}

TEST(JoinTest, MultipleElements) {
  std::vector<std::string_view> const parts{"a", "b", "c"};
  EXPECT_EQ(join(parts, ", "), "a, b, c");
}

TEST(JoinTest, EmptySeparator) {
  std::vector<std::string_view> const parts{"a", "b", "c"};
  EXPECT_EQ(join(parts, ""), "abc");
}

TEST(JoinTest, EmptyElements) {
  std::vector<std::string_view> const parts{"", "", ""};
  EXPECT_EQ(join(parts, "-"), "--");
}

TEST(JoinTest, LargeJoin) {
  std::vector<std::string_view> parts;
  for (int i = 0; i < 1000; ++i)
    parts.emplace_back("x");
  auto const result = join(parts, ",");
  EXPECT_EQ(result.size(), 1000u + 999u);
  EXPECT_EQ(result.front(), 'x');
  EXPECT_EQ(result.back(), 'x');
}

TEST(JoinTest, InitializerList) {
  auto const result = join({"a", "b", "c"}, "::");
  EXPECT_EQ(result, "a::b::c");
}

// ============================================================================
// replace_all
// ============================================================================

TEST(ReplaceAllTest, NotFound) {
  EXPECT_EQ(replace_all("hello", "x", "y"), "hello");
}

TEST(ReplaceAllTest, SingleOccurrence) {
  EXPECT_EQ(replace_all("hello", "ll", "yy"), "heyyo");
}

TEST(ReplaceAllTest, MultipleOccurrences) {
  EXPECT_EQ(replace_all("ababab", "ab", "x"), "xxx");
}

TEST(ReplaceAllTest, OverlappingFromNoMatch) {
  // "aaa" with "aa" → only replaces first "aa" (non-overlapping)
  EXPECT_EQ(replace_all("aaa", "aa", "b"), "ba");
}

TEST(ReplaceAllTest, EmptyFrom) {
  EXPECT_EQ(replace_all("hello", "", "x"), "hello");
}

TEST(ReplaceAllTest, ToLargerThanFrom) {
  EXPECT_EQ(replace_all("x x x", "x", "hello"), "hello hello hello");
}

TEST(ReplaceAllTest, EmptyString) {
  EXPECT_EQ(replace_all("", "x", "y"), "");
}

// ============================================================================
// to_lower / to_upper
// ============================================================================

TEST(ToLowerTest, Empty) {
  EXPECT_EQ(to_lower(""), "");
}

TEST(ToLowerTest, AlreadyLower) {
  EXPECT_EQ(to_lower("hello world"), "hello world");
}

TEST(ToLowerTest, Mixed) {
  EXPECT_EQ(to_lower("Hello World"), "hello world");
}

TEST(ToLowerTest, AllUpper) {
  EXPECT_EQ(to_lower("HELLO"), "hello");
}

TEST(ToLowerTest, NonAlpha) {
  EXPECT_EQ(to_lower("123!@#"), "123!@#");
}

TEST(ToLowerTest, ControlChars) {
  EXPECT_EQ(to_lower("A\nB\tC"), "a\nb\tc");
}

TEST(ToUpperTest, Empty) {
  EXPECT_EQ(to_upper(""), "");
}

TEST(ToUpperTest, AllLower) {
  EXPECT_EQ(to_upper("hello"), "HELLO");
}

TEST(ToUpperTest, Mixed) {
  EXPECT_EQ(to_upper("Hello World"), "HELLO WORLD");
}

TEST(ToUpperTest, NonAlpha) {
  EXPECT_EQ(to_upper("123!@#"), "123!@#");
}

// ============================================================================
// Round-trip / composition
// ============================================================================

TEST(CompositionTest, ReplaceAllThenToLower) {
  auto const result = to_lower(replace_all("Hello World", " ", "-"));
  EXPECT_EQ(result, "hello-world");
}

// ============================================================================
// String view lifetime tests
// ============================================================================

TEST(LifetimeTest, TrimViewPointsWithinSource) {
  std::string const s = "  hello  ";
  std::string_view const view = trim(s);
  // The view must point within the original string's buffer.
  EXPECT_GE(view.data(), s.data());
  EXPECT_LT(view.data() + view.size(), s.data() + s.size());
  EXPECT_EQ(view, "hello");
}

TEST(LifetimeTest, TrimViewOfTemporaryIsDangling) {
  // This compiles but is wrong — the temporary string dies at the semicolon.
  // Documenting the hazard: DO NOT DO THIS in real code.
  // auto view = trim(std::string("  temp  ")); // DANGER
  SUCCEED() << "Documented: never bind trim() result to a temporary string.";
}
