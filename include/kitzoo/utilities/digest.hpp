// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/digest.hpp
// Description: Declares SHA-256 and HMAC-SHA256 using OpenSSL EVP; available
//              when KZ_WITH_OPENSSL is enabled.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_DIGEST_HPP
#define KITZOO_UTILITIES_DIGEST_HPP

#if defined(KZ_WITH_OPENSSL) && KZ_WITH_OPENSSL

#include <kitzoo/core/macro.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace kitzoo::util {

using Sha256Digest = std::array<std::byte, 32>;

KZ_NODISCARD auto sha256(std::span<const std::byte> data) -> Sha256Digest;

KZ_NODISCARD auto hmac_sha256(std::span<const std::byte> key, std::span<const std::byte> data) -> Sha256Digest;

} // namespace kitzoo::util

#endif // KZ_WITH_OPENSSL

#endif // KITZOO_UTILITIES_DIGEST_HPP
