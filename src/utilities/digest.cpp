// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/utilities/digest.cpp
// Description: Implements SHA-256 and HMAC-SHA256 through OpenSSL.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/digest.hpp>

#include <limits>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdexcept>

namespace kitzoo::util {

namespace {

auto bytes(const std::span<const std::byte> data) -> const unsigned char* {
  return reinterpret_cast<const unsigned char*>(data.data());
}

} // namespace

auto sha256(const std::span<const std::byte> data) -> Sha256Digest {
  Sha256Digest digest{};
  unsigned int size = 0;
  if (EVP_Digest(bytes(data), data.size(), reinterpret_cast<unsigned char*>(digest.data()), &size, EVP_sha256(),
                 nullptr) != 1 ||
      size != digest.size())
    throw std::runtime_error{"SHA-256 computation failed"};
  return digest;
}

auto hmac_sha256(const std::span<const std::byte> key, const std::span<const std::byte> data) -> Sha256Digest {
  if (key.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    throw std::invalid_argument{"HMAC key is too large"};
  Sha256Digest digest{};
  unsigned int size = 0;
  if (HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), bytes(data), data.size(),
           reinterpret_cast<unsigned char*>(digest.data()), &size) == nullptr ||
      size != digest.size())
    throw std::runtime_error{"HMAC-SHA256 computation failed"};
  return digest;
}

} // namespace kitzoo::util
