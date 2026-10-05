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
  for (int const c : cards)
    std::printf(" %d", c);
  std::printf("\n");
  // UUID generation.
  std::printf("uuid:        %s\n", kitzoo::util::Uuid::random().to_string().c_str());

  // String parsing, trimming, case conversion, and hexadecimal encoding.
  {
    using namespace kitzoo::util;
    std::string input = "  42,Hello  ";
    auto const separator = input.find(',');
    auto const number = to_number<int>(std::string_view{input}.substr(0, separator));
    auto const text = trim(std::string_view{input}.substr(separator + 1));
    auto const encoded = hex_encode(std::as_bytes(std::span{text.data(), text.size()}));
    auto const decoded = hex_decode(encoded);
    std::printf("number=%d text=%.*s upper=%s hex=%s decoded=%zu bytes\n", number.value(),
                static_cast<int>(text.size()), text.data(), to_upper(text).c_str(), encoded.c_str(),
                decoded.value().size());
  }

  // Base64 encoding and decoding.
  {
    std::string_view const text = "kitzoo";
    auto const bytes = std::as_bytes(std::span{text.data(), text.size()});
    auto const encoded = kitzoo::util::base64_encode(bytes);
    auto const decoded = kitzoo::util::base64_decode(encoded);
    std::printf("base64=%s decoded=%zu bytes\n", encoded.c_str(), decoded.value().size());
  }

#if defined(KZ_WITH_OPENSSL) && KZ_WITH_OPENSSL
  // AES-CBC round trip with fixed demonstration inputs.
  {
    std::array<std::byte, 16> const key{};
    std::array<std::byte, 16> const iv{};
    std::string_view const text = "example payload";
    auto const bytes = std::as_bytes(std::span{text.data(), text.size()});
    auto const encrypted = kitzoo::util::aes_cbc_encrypt(bytes, key, iv);
    auto const decrypted = kitzoo::util::aes_cbc_decrypt(encrypted, key, iv);
    std::printf("AES encrypted=%zu bytes decoded=%zu bytes\n", encrypted.size(), decrypted.size());
  }
#endif

  return 0;
}
