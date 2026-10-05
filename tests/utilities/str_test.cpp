// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/utilities/str_test.cpp
// Description: Verifies string processing, numeric parsing, and hexadecimal conversion.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/str.hpp>

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

using namespace kitzoo::util;

// ============================================================================
// trim
// ============================================================================

TEST(StrTrimTest, Empty) {
  EXPECT_EQ(trim(""), "");
  EXPECT_EQ(trim_left(""), "");
  EXPECT_EQ(trim_right(""), "");
}

TEST(StrTrimTest, WhitespaceOnly) {
  EXPECT_EQ(trim("   "), "");
  EXPECT_EQ(trim("\t\n\r\f\v"), "");
}

TEST(StrTrimTest, NoWhitespace) {
  EXPECT_EQ(trim("hello"), "hello");
  EXPECT_EQ(trim_left("hello"), "hello");
  EXPECT_EQ(trim_right("hello"), "hello");
}

TEST(StrTrimTest, LeadingOnly) {
  EXPECT_EQ(trim("  hello"), "hello");
  EXPECT_EQ(trim_left("  hello"), "hello");
  EXPECT_EQ(trim_right("  hello"), "  hello");
}

TEST(StrTrimTest, TrailingOnly) {
  EXPECT_EQ(trim("hello  "), "hello");
  EXPECT_EQ(trim_left("hello  "), "hello  ");
  EXPECT_EQ(trim_right("hello  "), "hello");
}

TEST(StrTrimTest, BothSides) {
  EXPECT_EQ(trim("  hello  "), "hello");
}

TEST(StrTrimTest, MixedWhitespace) {
  EXPECT_EQ(trim("\t\n hello \r\f"), "hello");
}

TEST(StrTrimTest, SingleCharacter) {
  EXPECT_EQ(trim("x"), "x");
  EXPECT_EQ(trim_left(" "), "");
  EXPECT_EQ(trim_right(" "), "");
}

TEST(StrTrimTest, ViewOfOriginal) {
  std::string const original = "  hello  ";
  auto const view = trim(original);
  // view should point within original's buffer
  EXPECT_GE(view.data(), original.data());
  EXPECT_LT(view.data(), original.data() + original.size());
}

TEST(StrTrimTest, ConstexprWorks) {
  constexpr auto v = trim("  hi  ");
  static_assert(v == "hi");
}

// ============================================================================
// join
// ============================================================================

TEST(StrJoinTest, EmptySpan) {
  auto const result = join(std::span<std::string_view const>{}, ",");
  EXPECT_TRUE(result.empty());
}

TEST(StrJoinTest, SingleElement) {
  std::vector<std::string_view> const parts{"hello"};
  EXPECT_EQ(join(parts, ","), "hello");
}

TEST(StrJoinTest, MultipleElements) {
  std::vector<std::string_view> const parts{"a", "b", "c"};
  EXPECT_EQ(join(parts, ", "), "a, b, c");
}

TEST(StrJoinTest, EmptySeparator) {
  std::vector<std::string_view> const parts{"a", "b", "c"};
  EXPECT_EQ(join(parts, ""), "abc");
}

TEST(StrJoinTest, EmptyElements) {
  std::vector<std::string_view> const parts{"", "", ""};
  EXPECT_EQ(join(parts, "-"), "--");
}

TEST(StrJoinTest, LargeJoin) {
  std::vector<std::string_view> parts;
  for (int i = 0; i < 1000; ++i)
    parts.emplace_back("x");
  auto const result = join(parts, ",");
  EXPECT_EQ(result.size(), 1000u + 999u);
  EXPECT_EQ(result.front(), 'x');
  EXPECT_EQ(result.back(), 'x');
}

TEST(StrJoinTest, InitializerList) {
  auto const result = join({"a", "b", "c"}, "::");
  EXPECT_EQ(result, "a::b::c");
}

// ============================================================================
// replace_all
// ============================================================================

TEST(StrReplaceAllTest, NotFound) {
  EXPECT_EQ(replace_all("hello", "x", "y"), "hello");
}

TEST(StrReplaceAllTest, SingleOccurrence) {
  EXPECT_EQ(replace_all("hello", "ll", "yy"), "heyyo");
}

TEST(StrReplaceAllTest, MultipleOccurrences) {
  EXPECT_EQ(replace_all("ababab", "ab", "x"), "xxx");
}

TEST(StrReplaceAllTest, OverlappingFromNoMatch) {
  // "aaa" with "aa" → only replaces first "aa" (non-overlapping)
  EXPECT_EQ(replace_all("aaa", "aa", "b"), "ba");
}

TEST(StrReplaceAllTest, EmptyFrom) {
  EXPECT_EQ(replace_all("hello", "", "x"), "hello");
}

TEST(StrReplaceAllTest, ToLargerThanFrom) {
  EXPECT_EQ(replace_all("x x x", "x", "hello"), "hello hello hello");
}

TEST(StrReplaceAllTest, EmptyString) {
  EXPECT_EQ(replace_all("", "x", "y"), "");
}

// ============================================================================
// to_lower / to_upper
// ============================================================================

TEST(StrToLowerTest, Empty) {
  EXPECT_EQ(to_lower(""), "");
}

TEST(StrToLowerTest, AlreadyLower) {
  EXPECT_EQ(to_lower("hello world"), "hello world");
}

TEST(StrToLowerTest, Mixed) {
  EXPECT_EQ(to_lower("Hello World"), "hello world");
}

TEST(StrToLowerTest, AllUpper) {
  EXPECT_EQ(to_lower("HELLO"), "hello");
}

TEST(StrToLowerTest, NonAlpha) {
  EXPECT_EQ(to_lower("123!@#"), "123!@#");
}

TEST(StrToLowerTest, ControlChars) {
  EXPECT_EQ(to_lower("A\nB\tC"), "a\nb\tc");
}

TEST(StrToUpperTest, Empty) {
  EXPECT_EQ(to_upper(""), "");
}

TEST(StrToUpperTest, AllLower) {
  EXPECT_EQ(to_upper("hello"), "HELLO");
}

TEST(StrToUpperTest, Mixed) {
  EXPECT_EQ(to_upper("Hello World"), "HELLO WORLD");
}

TEST(StrToUpperTest, NonAlpha) {
  EXPECT_EQ(to_upper("123!@#"), "123!@#");
}

// ============================================================================
// Round-trip / composition
// ============================================================================

TEST(StrCompositionTest, ReplaceAllThenToLower) {
  auto const result = to_lower(replace_all("Hello World", " ", "-"));
  EXPECT_EQ(result, "hello-world");
}

// ============================================================================
// String view lifetime tests
// ============================================================================

TEST(StrLifetimeTest, TrimViewPointsWithinSource) {
  std::string const s = "  hello  ";
  std::string_view const view = trim(s);
  // The view must point within the original string's buffer.
  EXPECT_GE(view.data(), s.data());
  EXPECT_LT(view.data() + view.size(), s.data() + s.size());
  EXPECT_EQ(view, "hello");
}

// -- to_number (integral) ------------------------------------------------------

TEST(StrToNumberTest, BasicInt) {
  auto r = to_number<int>("42");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r.value(), 42);
}

TEST(StrToNumberTest, NegativeInt) {
  EXPECT_EQ(to_number<int>("-17").value(), -17);
}

TEST(StrToNumberTest, WhitespaceTolerated) {
  EXPECT_EQ(to_number<int>("  42  ").value(), 42);
}

TEST(StrToNumberTest, TrailingGarbageRejected) {
  auto r = to_number<int>("123abc");
  ASSERT_FALSE(r.has_value());
}

TEST(StrToNumberTest, EmptyRejected) {
  EXPECT_FALSE(to_number<int>("").has_value());
  EXPECT_FALSE(to_number<int>("   ").has_value());
}

TEST(StrToNumberTest, Overflow) {
  auto r = to_number<int>("9999999999999999999999");
  ASSERT_FALSE(r.has_value());
}

TEST(StrToNumberTest, Bases) {
  EXPECT_EQ(to_number<int>("ff", 16).value(), 255);
  EXPECT_EQ(to_number<int>("101", 2).value(), 5);
  EXPECT_EQ(to_number<int>("777", 8).value(), 511);
}

TEST(StrToNumberTest, UnsignedType) {
  EXPECT_EQ(to_number<unsigned int>("4294967295").value(), 4294967295u);
}

// -- to_number (floating point) -------------------------------------------------

TEST(StrToNumberTest, Double) {
  auto r = to_number<double>("3.14");
  ASSERT_TRUE(r.has_value());
  EXPECT_DOUBLE_EQ(r.value(), 3.14);
}

TEST(StrToNumberTest, DoubleNegative) {
  EXPECT_DOUBLE_EQ(to_number<double>("-0.5").value(), -0.5);
}

TEST(StrToNumberTest, DoubleGarbage) {
  EXPECT_FALSE(to_number<double>("3.14xyz").has_value());
}

// -- from_number ------------------------------------------------------------------

TEST(StrFromNumberTest, Int) {
  EXPECT_EQ(from_number(42), "42");
  EXPECT_EQ(from_number(-17), "-17");
  EXPECT_EQ(from_number(0), "0");
}

TEST(StrFromNumberTest, IntBase) {
  EXPECT_EQ(from_number(255, 16), "ff");
  EXPECT_EQ(from_number(5, 2), "101");
}

TEST(StrFromNumberTest, Double) {
  auto const s = from_number(1.5);
  EXPECT_TRUE(s == "1.5" || s == "1.500000");
}

TEST(StrConversionRoundTripTest, IntRoundTrip) {
  constexpr int kIntMin = -2147483647 - 1; // avoid literal overflow
  for (int v : {0, 1, -1, 42, -1000, 2147483647, kIntMin}) {
    EXPECT_EQ(to_number<int>(from_number(v)).value(), v);
  }
}

// -- hex ---------------------------------------------------------------------------

TEST(StrHexTest, EncodeBasic) {
  std::byte const data[] = {std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef}};
  EXPECT_EQ(hex_encode(data), "deadbeef");
}

TEST(StrHexTest, EncodeEmpty) {
  EXPECT_EQ(hex_encode({}), "");
}

TEST(StrHexTest, DecodeBasic) {
  auto r = hex_decode("deadbeef");
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(r.value().size(), 4u);
  EXPECT_EQ(r.value()[0], std::byte{0xde});
  EXPECT_EQ(r.value()[3], std::byte{0xef});
}

TEST(StrHexTest, DecodeUppercase) {
  EXPECT_TRUE(hex_decode("DEADBEEF").has_value());
}

TEST(StrHexTest, DecodeOddLengthFails) {
  auto r = hex_decode("abc");
  EXPECT_FALSE(r.has_value());
}

TEST(StrHexTest, DecodeInvalidCharFails) {
  EXPECT_FALSE(hex_decode("zz").has_value());
}

TEST(StrHexTest, RoundTrip) {
  std::vector<std::byte> data;
  for (int i = 0; i < 256; ++i)
    data.push_back(static_cast<std::byte>(i));
  auto const encoded = hex_encode(data);
  auto const decoded = hex_decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), data);
}

// -- misc ----------------------------------------------------------------------

TEST(StrComparisonTest, EqualsIgnoreCase) {
  EXPECT_TRUE(equals_ignore_case("Hello", "hELLO"));
  EXPECT_TRUE(equals_ignore_case("", ""));
  EXPECT_FALSE(equals_ignore_case("hello", "hell"));
  EXPECT_FALSE(equals_ignore_case("hello", "world"));
}
