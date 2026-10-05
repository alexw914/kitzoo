// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/aes.hpp
// Description: Declares AES-CBC and AES-GCM encryption and decryption using
//              OpenSSL EVP; available when KZ_WITH_OPENSSL is enabled.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_AES_HPP
#define KITZOO_UTILITIES_AES_HPP

#if defined(KZ_WITH_OPENSSL) && KZ_WITH_OPENSSL

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace kitzoo::util {

enum class Padding {
  Pkcs7,
  None,
};

KZ_NODISCARD auto aes_cbc_encrypt(std::span<const std::byte> data, std::span<const std::byte> key,
                                  std::span<const std::byte> iv, Padding padding = Padding::Pkcs7)
    -> std::vector<std::byte>;

KZ_NODISCARD auto aes_cbc_decrypt(std::span<const std::byte> ciphertext, std::span<const std::byte> key,
                                  std::span<const std::byte> iv, Padding padding = Padding::Pkcs7)
    -> std::vector<std::byte>;

inline constexpr std::size_t kAesGcmTagSize = 16;

// Requires a 12-byte IV that is never reused with the same key. Returns the
// ciphertext followed by the 16-byte authentication tag.
KZ_NODISCARD auto aes_gcm_encrypt(std::span<const std::byte> plaintext, std::span<const std::byte> key,
                                  std::span<const std::byte> iv, std::span<const std::byte> aad = {})
    -> std::vector<std::byte>;

// Returns nullopt when the data, tag or additional data fail authentication.
KZ_NODISCARD auto aes_gcm_decrypt(std::span<const std::byte> sealed, std::span<const std::byte> key,
                                  std::span<const std::byte> iv, std::span<const std::byte> aad = {})
    -> std::optional<std::vector<std::byte>>;

} // namespace kitzoo::util

#endif // KZ_WITH_OPENSSL

#endif // KITZOO_UTILITIES_AES_HPP
