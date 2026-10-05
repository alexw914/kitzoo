// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/utilities/base64_test.cpp
// Description: Verifies Base64 encoding, decoding, and invalid input handling.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/base64.hpp>

#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace kitzoo::util;

// -- base64 --------------------------------------------------------------------

TEST(Base64Test, EncodeEmpty) {
  EXPECT_EQ(base64_encode({}), "");
}

TEST(Base64Test, EncodeKnownVectors) {
  // RFC 4648 test vectors
  auto enc = [](std::string_view s) -> std::string {
    return base64_encode(std::span{reinterpret_cast<const std::byte*>(s.data()), s.size()});
  };
  EXPECT_EQ(enc("f"), "Zg==");
  EXPECT_EQ(enc("fo"), "Zm8=");
  EXPECT_EQ(enc("foo"), "Zm9v");
  EXPECT_EQ(enc("foob"), "Zm9vYg==");
  EXPECT_EQ(enc("fooba"), "Zm9vYmE=");
  EXPECT_EQ(enc("foobar"), "Zm9vYmFy");
}

TEST(Base64Test, DecodeKnownVectors) {
  auto dec = [](std::string_view b64) -> std::string {
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

TEST(Base64Test, DecodeRejectsNonCanonicalInput) {
  EXPECT_EQ(base64_decode("QQ==")->size(), 1u);
  EXPECT_EQ(base64_decode("QUI=")->size(), 2u);
  EXPECT_FALSE(base64_decode("QR==").has_value()); // unused bits set
  EXPECT_FALSE(base64_decode("QUJ=").has_value());
  EXPECT_FALSE(base64_decode("QQ=A").has_value()); // data after padding
}

TEST(Base64Test, RoundTrip) {
  std::vector<std::byte> data;
  for (int i = 0; i < 256; ++i)
    data.push_back(static_cast<std::byte>(i));
  const auto encoded = base64_encode(data);
  const auto decoded = base64_decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), data);
}
