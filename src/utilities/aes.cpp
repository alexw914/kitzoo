// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/utilities/aes.cpp
// Description: Implements AES-CBC and AES-GCM encryption and decryption through
//              OpenSSL EVP, including padding, tag validation and error handling.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/utilities/aes.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>

namespace kitzoo::util {

namespace {

using EvpCtx = memory::UniquePtr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

constexpr std::size_t kGcmIvSize = 12;

auto cipher_for(const std::size_t key_size, const bool gcm) -> const EVP_CIPHER* {
  switch (key_size) {
  case 16:
    return gcm ? EVP_aes_128_gcm() : EVP_aes_128_cbc();
  case 24:
    return gcm ? EVP_aes_192_gcm() : EVP_aes_192_cbc();
  case 32:
    return gcm ? EVP_aes_256_gcm() : EVP_aes_256_cbc();
  default:
    throw std::invalid_argument{"AES key must be 16, 24, or 32 bytes"};
  }
}

auto bytes(const std::span<const std::byte> data) -> const unsigned char* {
  return reinterpret_cast<const unsigned char*>(data.data());
}

auto init(const bool encrypt, const EVP_CIPHER* cipher, const std::span<const std::byte> key,
          const std::span<const std::byte> iv) -> EvpCtx {
  EvpCtx ctx{EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free};
  if (!ctx)
    throw std::runtime_error{"EVP_CIPHER_CTX_new failed"};
  const int ok = encrypt ? EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, bytes(key), bytes(iv))
                         : EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, bytes(key), bytes(iv));
  if (ok != 1)
    throw std::runtime_error{"AES cipher initialization failed"};
  return ctx;
}

// OpenSSL lengths are int; chunks leave room for one block of buffered output.
// A null output feeds GCM additional authenticated data.
auto update(EVP_CIPHER_CTX* ctx, const bool encrypt, const std::span<const std::byte> input, unsigned char* out)
    -> std::size_t {
  constexpr auto kMaxChunk = static_cast<std::size_t>(std::numeric_limits<int>::max() - 64);
  std::size_t written = 0;
  for (std::size_t offset = 0; offset < input.size(); offset += kMaxChunk) {
    const auto chunk = static_cast<int>(std::min(kMaxChunk, input.size() - offset));
    auto* const dst = out == nullptr ? nullptr : out + written;
    int out_len = 0;
    const int ok = encrypt ? EVP_EncryptUpdate(ctx, dst, &out_len, bytes(input) + offset, chunk)
                           : EVP_DecryptUpdate(ctx, dst, &out_len, bytes(input) + offset, chunk);
    if (ok != 1)
      throw std::runtime_error{"AES cipher update failed"};
    written += static_cast<std::size_t>(out_len);
  }
  return written;
}

auto run_cbc(const bool encrypt, const std::span<const std::byte> input, const std::span<const std::byte> key,
             const std::span<const std::byte> iv, const Padding padding) -> std::vector<std::byte> {
  const EVP_CIPHER* const cipher = cipher_for(key.size(), false);
  if (iv.size() != static_cast<std::size_t>(EVP_CIPHER_iv_length(cipher)))
    throw std::invalid_argument{"AES-CBC IV must be 16 bytes"};
  const auto block_size = static_cast<std::size_t>(EVP_CIPHER_block_size(cipher));
  if (padding == Padding::None && input.size() % block_size != 0)
    throw std::invalid_argument{"AES-CBC input must be block-aligned without padding"};

  auto ctx = init(encrypt, cipher, key, iv);
  EVP_CIPHER_CTX_set_padding(ctx.get(), padding == Padding::Pkcs7 ? 1 : 0);

  std::vector<std::byte> out(input.size() + block_size);
  auto* const dst = reinterpret_cast<unsigned char*>(out.data());
  const auto written = update(ctx.get(), encrypt, input, dst);
  int final_len = 0;
  const int ok = encrypt ? EVP_EncryptFinal_ex(ctx.get(), dst + written, &final_len)
                         : EVP_DecryptFinal_ex(ctx.get(), dst + written, &final_len);
  if (ok != 1)
    throw std::runtime_error{"AES cipher finalization failed"};
  out.resize(written + static_cast<std::size_t>(final_len));
  return out;
}

auto check_gcm_iv(const std::span<const std::byte> iv) -> void {
  if (iv.size() != kGcmIvSize)
    throw std::invalid_argument{"AES-GCM IV must be 12 bytes"};
}

} // namespace

auto aes_cbc_encrypt(const std::span<const std::byte> data, const std::span<const std::byte> key,
                     const std::span<const std::byte> iv, const Padding padding) -> std::vector<std::byte> {
  return run_cbc(true, data, key, iv, padding);
}

auto aes_cbc_decrypt(const std::span<const std::byte> ciphertext, const std::span<const std::byte> key,
                     const std::span<const std::byte> iv, const Padding padding) -> std::vector<std::byte> {
  return run_cbc(false, ciphertext, key, iv, padding);
}

auto aes_gcm_encrypt(const std::span<const std::byte> plaintext, const std::span<const std::byte> key,
                     const std::span<const std::byte> iv, const std::span<const std::byte> aad)
    -> std::vector<std::byte> {
  const EVP_CIPHER* const cipher = cipher_for(key.size(), true);
  check_gcm_iv(iv);
  auto ctx = init(true, cipher, key, iv);
  update(ctx.get(), true, aad, nullptr);

  std::vector<std::byte> out(plaintext.size() + kAesGcmTagSize);
  auto* const dst = reinterpret_cast<unsigned char*>(out.data());
  const auto written = update(ctx.get(), true, plaintext, dst);
  int final_len = 0;
  if (EVP_EncryptFinal_ex(ctx.get(), dst + written, &final_len) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kAesGcmTagSize),
                          dst + written + static_cast<std::size_t>(final_len)) != 1)
    throw std::runtime_error{"AES-GCM finalization failed"};
  out.resize(written + static_cast<std::size_t>(final_len) + kAesGcmTagSize);
  return out;
}

auto aes_gcm_decrypt(const std::span<const std::byte> sealed, const std::span<const std::byte> key,
                     const std::span<const std::byte> iv, const std::span<const std::byte> aad)
    -> std::optional<std::vector<std::byte>> {
  const EVP_CIPHER* const cipher = cipher_for(key.size(), true);
  check_gcm_iv(iv);
  if (sealed.size() < kAesGcmTagSize)
    return std::nullopt;
  const auto ciphertext = sealed.first(sealed.size() - kAesGcmTagSize);
  const auto tag = sealed.last(kAesGcmTagSize);

  auto ctx = init(false, cipher, key, iv);
  update(ctx.get(), false, aad, nullptr);
  std::vector<std::byte> out(ciphertext.size() + kAesGcmTagSize);
  auto* const dst = reinterpret_cast<unsigned char*>(out.data());
  const auto written = update(ctx.get(), false, ciphertext, dst);
  // OpenSSL takes a non-const pointer but only reads the expected tag.
  auto* const expected = const_cast<unsigned char*>(bytes(tag));
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kAesGcmTagSize), expected) != 1)
    throw std::runtime_error{"AES-GCM tag setup failed"};
  int final_len = 0;
  if (EVP_DecryptFinal_ex(ctx.get(), dst + written, &final_len) != 1)
    return std::nullopt;
  out.resize(written + static_cast<std::size_t>(final_len));
  return out;
}

} // namespace kitzoo::util
