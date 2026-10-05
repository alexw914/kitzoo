// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/utilities/digest_test.cpp
// Description: Verifies SHA-256 and HMAC-SHA256 against published test vectors.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/digest.hpp>
#include <kitzoo/utilities/str.hpp>

#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

namespace {

auto text(std::string_view value) -> std::span<const std::byte> {
  return std::as_bytes(std::span{value.data(), value.size()});
}

auto hex(const kitzoo::util::Sha256Digest& digest) -> std::string {
  return kitzoo::util::hex_encode(digest);
}

// FIPS 180-2 examples.
TEST(DigestTest, Sha256MatchesReferenceVectors) {
  EXPECT_EQ(hex(kitzoo::util::sha256(text("abc"))), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(hex(kitzoo::util::sha256(text(""))), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// RFC 4231, test case 2.
TEST(DigestTest, HmacSha256MatchesRfc4231) {
  EXPECT_EQ(hex(kitzoo::util::hmac_sha256(text("Jefe"), text("what do ya want for nothing?"))),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

} // namespace
