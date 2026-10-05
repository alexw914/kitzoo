// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/utilities.cpp
// Description: Demonstrates string processing, encoding, random helpers, and UUIDs.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities.hpp>

#include <array>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

auto main() -> int {
  // random: thread-local engine, no setup required. NOT cryptographically
  // secure; use a CSPRNG for secrets.
  std::printf("die roll:    %d\n", kitzoo::util::random_int(1, 6));
  std::printf("probability: %.3f\n", kitzoo::util::random_real(0.0, 1.0));
  std::printf("token:       %s\n", kitzoo::util::random_string(12).c_str());

  std::vector<int> cards{1, 2, 3, 4, 5, 6};
  kitzoo::util::shuffle(cards);
  std::printf("shuffled:   ");
  for (const int c : cards)
    std::printf(" %d", c);
  std::printf("\n");
  // UUID generation.
  std::printf("uuid:        %s\n", kitzoo::util::Uuid::random().to_string().c_str());

  // String splitting, parsing, trimming, case conversion, and hexadecimal encoding.
  {
    using namespace kitzoo::util;
    const std::string input = "  42,Hello  ";
    const auto fields = split(input, ",");
    const auto number = to_number<int>(fields[0]);
    const auto text = trim(fields[1]);
    const auto encoded = hex_encode(std::as_bytes(std::span{text.data(), text.size()}));
    const auto decoded = hex_decode(encoded);
    std::printf("number=%d text=%.*s upper=%s hex=%s decoded=%zu bytes\n", number.value(),
                static_cast<int>(text.size()), text.data(), to_upper(text).c_str(), encoded.c_str(),
                decoded.value().size());
  }

  // Base64 encoding and decoding.
  {
    const std::string_view text = "kitzoo";
    const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
    const auto encoded = kitzoo::util::base64_encode(bytes);
    const auto decoded = kitzoo::util::base64_decode(encoded);
    std::printf("base64=%s decoded=%zu bytes\n", encoded.c_str(), decoded.value().size());
  }

#if defined(KZ_WITH_OPENSSL) && KZ_WITH_OPENSSL
  // AES-CBC round trip with fixed demonstration inputs.
  {
    const std::array<std::byte, 16> key{};
    const std::array<std::byte, 16> iv{};
    const std::string_view text = "example payload";
    const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
    const auto encrypted = kitzoo::util::aes_cbc_encrypt(bytes, key, iv);
    const auto decrypted = kitzoo::util::aes_cbc_decrypt(encrypted, key, iv);
    std::printf("AES encrypted=%zu bytes decoded=%zu bytes\n", encrypted.size(), decrypted.size());
  }

  // Authenticated encryption and message digests.
  {
    const std::array<std::byte, 32> key{};
    const std::array<std::byte, 12> nonce{};
    const std::string_view text = "sensor frame";
    const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
    auto sealed = kitzoo::util::aes_gcm_encrypt(bytes, key, nonce);
    std::printf("AES-GCM sealed=%zu bytes, authentic=%s\n", sealed.size(),
                kitzoo::util::aes_gcm_decrypt(sealed, key, nonce) ? "yes" : "no");
    sealed.front() ^= std::byte{0x01};
    std::printf("tampered message rejected=%s\n", kitzoo::util::aes_gcm_decrypt(sealed, key, nonce) ? "no" : "yes");
    std::printf("sha256=%s\n", kitzoo::util::hex_encode(kitzoo::util::sha256(bytes)).c_str());
    std::printf("hmac=%s\n", kitzoo::util::hex_encode(kitzoo::util::hmac_sha256(key, bytes)).c_str());
  }
#endif

  return 0;
}
