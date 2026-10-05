// ---------------------------------------------------------------------------
// kitzoo/crypto AES tests (OpenSSL EVP round-trips)
// ---------------------------------------------------------------------------

#include <kitzoo/crypto/aes.hpp>
#include <kitzoo/string/convert_extra.hpp>

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

using namespace kitzoo;
using namespace kitzoo::crypto;

namespace {

auto bytes_of(std::string_view s) {
  return std::span{reinterpret_cast<std::byte const*>(s.data()), s.size()};
}

auto to_string_view(std::vector<std::byte> const& v) -> std::string {
  std::string out(v.size(), '\0');
  for (std::size_t i = 0; i < v.size(); ++i)
    out[i] = static_cast<char>(v[i]);
  return out;
}

// NIST-ish known answer for AES-128-CBC:
// key=2b7e151628aed2a6abf7158809cf4f3c iv=000102030405060708090a0b0c0d0e0f
// plaintext "6bc1bee22e409f96e93d7e117393172a" ->
// ciphertext "7649abac8119b246cee98e9b12e9197d"
constexpr std::byte kKey128[16] = {std::byte{0x2b}, std::byte{0x7e}, std::byte{0x15}, std::byte{0x16},
                                   std::byte{0x28}, std::byte{0xae}, std::byte{0xd2}, std::byte{0xa6},
                                   std::byte{0xab}, std::byte{0xf7}, std::byte{0x15}, std::byte{0x88},
                                   std::byte{0x09}, std::byte{0xcf}, std::byte{0x4f}, std::byte{0x3c}};
constexpr std::byte kIv[16] = {std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
                               std::byte{0x04}, std::byte{0x05}, std::byte{0x06}, std::byte{0x07},
                               std::byte{0x08}, std::byte{0x09}, std::byte{0x0a}, std::byte{0x0b},
                               std::byte{0x0c}, std::byte{0x0d}, std::byte{0x0e}, std::byte{0x0f}};

} // namespace

TEST(AesTest, EncryptDecryptRoundTrip128) {
  std::string const plaintext = "the quick brown fox jumps over the lazy dog";
  auto const enc = aes_cbc_encrypt(bytes_of(plaintext), kKey128, kIv);
  EXPECT_NE(enc.size(), plaintext.size()); // padded to block size

  auto const dec = aes_cbc_decrypt(enc, kKey128, kIv);
  EXPECT_EQ(to_string_view(dec), plaintext);
}

TEST(AesTest, KnownAnswerAes128CbcNoPadding) {
  // Single block, PKCS7 disabled → exact NIST vector
  auto const pt = str::hex_decode("6bc1bee22e409f96e93d7e117393172a");
  ASSERT_TRUE(pt.has_value());
  auto const enc = aes_cbc_encrypt(*pt, kKey128, kIv, Padding::None);
  EXPECT_EQ(str::hex_encode(enc), "7649abac8119b246cee98e9b12e9197d");
}

TEST(AesTest, Aes256RoundTrip) {
  std::array<std::byte, 32> key{};
  key.fill(std::byte{0x42});
  std::string const plaintext = "AES-256 test payload with some length";
  auto const enc = aes_cbc_encrypt(bytes_of(plaintext), key, kIv);
  auto const dec = aes_cbc_decrypt(enc, key, kIv);
  EXPECT_EQ(to_string_view(dec), plaintext);
}

TEST(AesTest, BadKeySizeFails) {
  std::array<std::byte, 7> bad_key{};
  EXPECT_THROW(static_cast<void>(aes_cbc_encrypt(bytes_of("data"), bad_key, kIv)), std::invalid_argument);
}

TEST(AesTest, BadIvSizeFails) {
  std::array<std::byte, 8> bad_iv{};
  EXPECT_THROW(static_cast<void>(aes_cbc_encrypt(bytes_of("data"), kKey128, bad_iv)), std::invalid_argument);
}

TEST(AesTest, NoPaddingUnalignedFails) {
  EXPECT_THROW(static_cast<void>(aes_cbc_encrypt(bytes_of("not block aligned"), kKey128, kIv, Padding::None)),
               std::invalid_argument);
}

TEST(AesTest, DecryptCorruptedFails) {
  std::string const plaintext = "padding oracle check payload!";
  auto enc = aes_cbc_encrypt(bytes_of(plaintext), kKey128, kIv);

  auto corrupted = enc;
  corrupted.back() ^= std::byte{0xFF}; // break padding
  EXPECT_THROW(static_cast<void>(aes_cbc_decrypt(corrupted, kKey128, kIv)), std::runtime_error);
}

TEST(AesTest, EmptyPlaintext) {
  auto const enc = aes_cbc_encrypt(bytes_of(""), kKey128, kIv);
  EXPECT_EQ(enc.size(), 16u); // one full padding block
  auto const dec = aes_cbc_decrypt(enc, kKey128, kIv);
  EXPECT_TRUE(dec.empty());
}
