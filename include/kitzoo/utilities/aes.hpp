// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/aes.hpp
// Description: Declares AES-CBC encryption and decryption using OpenSSL EVP;
//              available when KZ_WITH_OPENSSL is enabled.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_AES_HPP
#define KITZOO_UTILITIES_AES_HPP

#if defined(KZ_WITH_OPENSSL) && KZ_WITH_OPENSSL

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace kitzoo::util {

enum class Padding {
  Pkcs7,
  None,
};

KZ_NODISCARD auto aes_cbc_encrypt(std::span<std::byte const> data, std::span<std::byte const> key,
                                  std::span<std::byte const> iv, Padding padding = Padding::Pkcs7)
    -> std::vector<std::byte>;

KZ_NODISCARD auto aes_cbc_decrypt(std::span<std::byte const> ciphertext, std::span<std::byte const> key,
                                  std::span<std::byte const> iv, Padding padding = Padding::Pkcs7)
    -> std::vector<std::byte>;

} // namespace kitzoo::util

#endif // KZ_WITH_OPENSSL

#endif // KITZOO_UTILITIES_AES_HPP
