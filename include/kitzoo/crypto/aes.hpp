// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/crypto/aes.hpp
// Description: Declares AES-CBC encryption and decryption using OpenSSL EVP;
//              available when the crypto target is enabled.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CRYPTO_AES_HPP
#define KITZOO_CRYPTO_AES_HPP

#if !defined(KZ_WITH_OPENSSL)
#error "kitzoo/crypto/aes.hpp requires the kitzoo::crypto target"
#endif

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace kitzoo::crypto {

enum class Padding {
  Pkcs7,
  None,
};

KZ_NODISCARD auto aes_cbc_encrypt(std::span<std::byte const> data, std::span<std::byte const> key,
                                  std::span<std::byte const> iv,
                                  Padding padding = Padding::Pkcs7) -> std::vector<std::byte>;

KZ_NODISCARD auto aes_cbc_decrypt(std::span<std::byte const> ciphertext, std::span<std::byte const> key,
                                  std::span<std::byte const> iv,
                                  Padding padding = Padding::Pkcs7) -> std::vector<std::byte>;

} // namespace kitzoo::crypto

#endif // KITZOO_CRYPTO_AES_HPP
