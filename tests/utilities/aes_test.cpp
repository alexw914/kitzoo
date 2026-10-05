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

namespace {
using kitzoo::util::aes_gcm_decrypt;
using kitzoo::util::aes_gcm_encrypt;

// GCM specification (McGrew and Viega), test case 2.
TEST(AesTest, GcmMatchesReferenceVectorWithoutAad) {
  const auto key = bytes("00000000000000000000000000000000");
  const auto iv = bytes("000000000000000000000000");
  const auto plain = bytes("00000000000000000000000000000000");
  const auto sealed = aes_gcm_encrypt(plain, key, iv);
  EXPECT_EQ(kitzoo::util::hex_encode(sealed), "0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf");
  EXPECT_EQ(aes_gcm_decrypt(sealed, key, iv), plain);
}

// GCM specification, test case 4.
TEST(AesTest, GcmMatchesReferenceVectorWithAad) {
  const auto key = bytes("feffe9928665731c6d6a8f9467308308");
  const auto iv = bytes("cafebabefacedbaddecaf888");
  const auto plain = bytes("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a"
                           "6b525b16aedf5aa0de657ba637b39");
  const auto aad = bytes("feedfacedeadbeeffeedfacedeadbeefabaddad2");
  const auto sealed = aes_gcm_encrypt(plain, key, iv, aad);
  EXPECT_EQ(kitzoo::util::hex_encode(sealed), "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b2"
                                              "5466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091"
                                              "5bc94fbc3221a5db94fae95ae7121a47");
  EXPECT_EQ(aes_gcm_decrypt(sealed, key, iv, aad), plain);
}

TEST(AesTest, GcmRejectsTamperingAndInvalidArguments) {
  const auto key = std::vector<std::byte>(32, std::byte{0x11});
  const auto iv = std::vector<std::byte>(12, std::byte{0x22});
  const auto aad = bytes("0102");
  const auto plain = bytes("00112233445566778899");
  auto sealed = aes_gcm_encrypt(plain, key, iv, aad);
  ASSERT_EQ(sealed.size(), plain.size() + kitzoo::util::kAesGcmTagSize);
  EXPECT_EQ(aes_gcm_decrypt(sealed, key, iv, bytes("0103")), std::nullopt);
  sealed.front() ^= std::byte{0x01};
  EXPECT_EQ(aes_gcm_decrypt(sealed, key, iv, aad), std::nullopt);
  sealed.front() ^= std::byte{0x01};
  sealed.back() ^= std::byte{0x01};
  EXPECT_EQ(aes_gcm_decrypt(sealed, key, iv, aad), std::nullopt);
  EXPECT_EQ(aes_gcm_decrypt(std::span{sealed}.first(4), key, iv, aad), std::nullopt);
  EXPECT_THROW((void)aes_gcm_encrypt(plain, key, std::vector<std::byte>(16)), std::invalid_argument);
  EXPECT_THROW((void)aes_gcm_encrypt(plain, std::vector<std::byte>(20), iv), std::invalid_argument);
}
} // namespace
