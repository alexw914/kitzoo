// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/utilities/aes_test.cpp
// Description: Verifies AES-CBC encryption against reference vectors and argument checks.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/aes.hpp>
#include <kitzoo/utilities/str.hpp>

#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using kitzoo::util::aes_cbc_decrypt;
using kitzoo::util::aes_cbc_encrypt;
using kitzoo::util::Padding;

auto bytes(std::string_view hex) -> std::vector<std::byte> {
  return kitzoo::util::hex_decode(hex).value();
}

// NIST SP 800-38A, F.2.1 CBC-AES128.Encrypt, first block.
TEST(AesTest, MatchesNistVectorWithoutPadding) {
  const auto key = bytes("2b7e151628aed2a6abf7158809cf4f3c");
  const auto iv = bytes("000102030405060708090a0b0c0d0e0f");
  const auto plain = bytes("6bc1bee22e409f96e93d7e117393172a");
  const auto cipher = aes_cbc_encrypt(plain, key, iv, Padding::None);
  EXPECT_EQ(kitzoo::util::hex_encode(cipher), "7649abac8119b246cee98e9b12e9197d");
  EXPECT_EQ(aes_cbc_decrypt(cipher, key, iv, Padding::None), plain);
}

TEST(AesTest, Pkcs7RoundTripForEveryKeySize) {
  const auto iv = std::vector<std::byte>(16, std::byte{0x24});
  const auto plain = bytes("00112233445566778899aabbccddeeff0011");
  for (std::size_t size : {16U, 24U, 32U}) {
    const auto key = std::vector<std::byte>(size, std::byte{0x5a});
    const auto cipher = aes_cbc_encrypt(plain, key, iv);
    EXPECT_EQ(cipher.size(), 32U);
    EXPECT_EQ(aes_cbc_decrypt(cipher, key, iv), plain);
  }
}

TEST(AesTest, RejectsInvalidArguments) {
  const auto key = std::vector<std::byte>(16);
  const auto iv = std::vector<std::byte>(16);
  const auto block = std::vector<std::byte>(16);
  EXPECT_THROW((void)aes_cbc_encrypt(block, std::vector<std::byte>(15), iv), std::invalid_argument);
  EXPECT_THROW((void)aes_cbc_encrypt(block, key, std::vector<std::byte>(8)), std::invalid_argument);
  EXPECT_THROW((void)aes_cbc_encrypt(std::vector<std::byte>(15), key, iv, Padding::None), std::invalid_argument);
  EXPECT_THROW((void)aes_cbc_decrypt(std::vector<std::byte>(15), key, iv), std::runtime_error);
}
} // namespace
