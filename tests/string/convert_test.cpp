// ---------------------------------------------------------------------------
// kitzoo/string: to_number / from_number / hex / base64 / misc tests
// ---------------------------------------------------------------------------

#include <kitzoo/string/convert.hpp>
#include <kitzoo/string/convert_extra.hpp>

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::str;

// -- to_number (integral) ------------------------------------------------------

TEST(ToNumberTest, BasicInt) {
  auto r = to_number<int>("42");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r.value(), 42);
}

TEST(ToNumberTest, NegativeInt) {
  EXPECT_EQ(to_number<int>("-17").value(), -17);
}

TEST(ToNumberTest, WhitespaceTolerated) {
  EXPECT_EQ(to_number<int>("  42  ").value(), 42);
}

TEST(ToNumberTest, TrailingGarbageRejected) {
  auto r = to_number<int>("123abc");
  ASSERT_FALSE(r.has_value());
}

TEST(ToNumberTest, EmptyRejected) {
  EXPECT_FALSE(to_number<int>("").has_value());
  EXPECT_FALSE(to_number<int>("   ").has_value());
}

TEST(ToNumberTest, Overflow) {
  auto r = to_number<int>("9999999999999999999999");
  ASSERT_FALSE(r.has_value());
}

TEST(ToNumberTest, Bases) {
  EXPECT_EQ(to_number<int>("ff", 16).value(), 255);
  EXPECT_EQ(to_number<int>("101", 2).value(), 5);
  EXPECT_EQ(to_number<int>("777", 8).value(), 511);
}

TEST(ToNumberTest, UnsignedType) {
  EXPECT_EQ(to_number<unsigned int>("4294967295").value(), 4294967295u);
}

// -- to_number (floating point) -------------------------------------------------

TEST(ToNumberTest, Double) {
  auto r = to_number<double>("3.14");
  ASSERT_TRUE(r.has_value());
  EXPECT_DOUBLE_EQ(r.value(), 3.14);
}

TEST(ToNumberTest, DoubleNegative) {
  EXPECT_DOUBLE_EQ(to_number<double>("-0.5").value(), -0.5);
}

TEST(ToNumberTest, DoubleGarbage) {
  EXPECT_FALSE(to_number<double>("3.14xyz").has_value());
}

// -- from_number ------------------------------------------------------------------

TEST(FromNumberTest, Int) {
  EXPECT_EQ(from_number(42), "42");
  EXPECT_EQ(from_number(-17), "-17");
  EXPECT_EQ(from_number(0), "0");
}

TEST(FromNumberTest, IntBase) {
  EXPECT_EQ(from_number(255, 16), "ff");
  EXPECT_EQ(from_number(5, 2), "101");
}

TEST(FromNumberTest, Double) {
  auto const s = from_number(1.5);
  EXPECT_TRUE(s == "1.5" || s == "1.500000");
}

TEST(ConversionRoundTrip, IntRoundTrip) {
  constexpr int kIntMin = -2147483647 - 1; // avoid literal overflow
  for (int v : {0, 1, -1, 42, -1000, 2147483647, kIntMin}) {
    EXPECT_EQ(to_number<int>(from_number(v)).value(), v);
  }
}

// -- hex ---------------------------------------------------------------------------

TEST(HexTest, EncodeBasic) {
  std::byte const data[] = {std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef}};
  EXPECT_EQ(hex_encode(data), "deadbeef");
}

TEST(HexTest, EncodeEmpty) {
  EXPECT_EQ(hex_encode({}), "");
}

TEST(HexTest, DecodeBasic) {
  auto r = hex_decode("deadbeef");
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(r.value().size(), 4u);
  EXPECT_EQ(r.value()[0], std::byte{0xde});
  EXPECT_EQ(r.value()[3], std::byte{0xef});
}

TEST(HexTest, DecodeUppercase) {
  EXPECT_TRUE(hex_decode("DEADBEEF").has_value());
}

TEST(HexTest, DecodeOddLengthFails) {
  auto r = hex_decode("abc");
  EXPECT_FALSE(r.has_value());
}

TEST(HexTest, DecodeInvalidCharFails) {
  EXPECT_FALSE(hex_decode("zz").has_value());
}

TEST(HexTest, RoundTrip) {
  std::vector<std::byte> data;
  for (int i = 0; i < 256; ++i)
    data.push_back(static_cast<std::byte>(i));
  auto const encoded = hex_encode(data);
  auto const decoded = hex_decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), data);
}

// -- base64 --------------------------------------------------------------------

TEST(Base64Test, EncodeEmpty) {
  EXPECT_EQ(base64_encode({}), "");
}

TEST(Base64Test, EncodeKnownVectors) {
  // RFC 4648 test vectors
  auto enc = [](std::string_view s) {
    return base64_encode(std::span{reinterpret_cast<std::byte const*>(s.data()), s.size()});
  };
  EXPECT_EQ(enc("f"), "Zg==");
  EXPECT_EQ(enc("fo"), "Zm8=");
  EXPECT_EQ(enc("foo"), "Zm9v");
  EXPECT_EQ(enc("foob"), "Zm9vYg==");
  EXPECT_EQ(enc("fooba"), "Zm9vYmE=");
  EXPECT_EQ(enc("foobar"), "Zm9vYmFy");
}

TEST(Base64Test, DecodeKnownVectors) {
  auto dec = [](std::string_view b64) {
    auto r = base64_decode(b64);
    EXPECT_TRUE(r.has_value());
    std::string out;
    for (auto b : r.value())
      out.push_back(static_cast<char>(b));
    return out;
  };
  EXPECT_EQ(dec("Zg=="), "f");
  EXPECT_EQ(dec("Zm8="), "fo");
  EXPECT_EQ(dec("Zm9v"), "foo");
  EXPECT_EQ(dec("Zm9vYmFy"), "foobar");
}

TEST(Base64Test, DecodeBadLength) {
  EXPECT_FALSE(base64_decode("Zg=").has_value()); // not multiple of 4
}

TEST(Base64Test, DecodeInvalidChar) {
  EXPECT_FALSE(base64_decode("Zm9!").has_value());
}

TEST(Base64Test, DecodePaddingOnlyAtEnd) {
  EXPECT_FALSE(base64_decode("Zg==Zm9v").has_value()); // padding mid-input
}

TEST(Base64Test, RoundTrip) {
  std::vector<std::byte> data;
  for (int i = 0; i < 256; ++i)
    data.push_back(static_cast<std::byte>(i));
  auto const encoded = base64_encode(data);
  auto const decoded = base64_decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), data);
}

// -- misc ----------------------------------------------------------------------

TEST(StringExtraTest, EqualsIgnoreCase) {
  EXPECT_TRUE(equals_ignore_case("Hello", "hELLO"));
  EXPECT_TRUE(equals_ignore_case("", ""));
  EXPECT_FALSE(equals_ignore_case("hello", "hell"));
  EXPECT_FALSE(equals_ignore_case("hello", "world"));
}
