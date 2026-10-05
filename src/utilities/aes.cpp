// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/utilities/aes.cpp
// Description: Implements AES-CBC encryption and decryption through OpenSSL
//              EVP, including padding validation and error handling.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/utilities/aes.hpp>

#include <memory>
#include <openssl/evp.h>
#include <stdexcept>

namespace kitzoo::util {

namespace {

using EvpCtx = memory::UniquePtr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

auto cipher_for(const std::size_t key_size) -> const EVP_CIPHER* {
  switch (key_size) {
  case 16:
    return EVP_aes_128_cbc();
  case 24:
    return EVP_aes_192_cbc();
  case 32:
    return EVP_aes_256_cbc();
  default:
    return nullptr;
  }
}

auto run(const bool encrypt, const std::span<const std::byte> input, const std::span<const std::byte> key,
         const std::span<const std::byte> iv, const Padding padding) -> std::vector<std::byte> {
  const EVP_CIPHER* const cipher = cipher_for(key.size());
  if (cipher == nullptr) {
    throw std::invalid_argument{"AES key must be 16, 24, or 32 bytes"};
  }
  const auto iv_len = static_cast<std::size_t>(EVP_CIPHER_iv_length(cipher));
  if (iv.size() != iv_len) {
    throw std::invalid_argument{"AES-CBC IV must be 16 bytes"};
  }
  const auto block_size = static_cast<std::size_t>(EVP_CIPHER_block_size(cipher));
  if (padding == Padding::None && input.size() % block_size != 0) {
    throw std::invalid_argument{"AES-CBC input must be block-aligned without padding"};
  }

  EvpCtx ctx{EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free};
  if (!ctx)
    throw std::runtime_error{"EVP_CIPHER_CTX_new failed"};

  const auto* const k = reinterpret_cast<const unsigned char*>(key.data());
  const auto* const v = reinterpret_cast<const unsigned char*>(iv.data());

  const int init_ok = encrypt ? EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, k, v)
                              : EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, k, v);
  if (init_ok != 1)
    throw std::runtime_error{"AES cipher initialization failed"};

  EVP_CIPHER_CTX_set_padding(ctx.get(), padding == Padding::Pkcs7 ? 1 : 0);

  std::vector<std::byte> out(input.size() + block_size);
  int out_len = 0;
  int final_len = 0;

  const auto* const in = reinterpret_cast<const unsigned char*>(input.data());
  auto* const dst = reinterpret_cast<unsigned char*>(out.data());

  const int update_ok = encrypt ? EVP_EncryptUpdate(ctx.get(), dst, &out_len, in, static_cast<int>(input.size()))
                                : EVP_DecryptUpdate(ctx.get(), dst, &out_len, in, static_cast<int>(input.size()));
  if (update_ok != 1)
    throw std::runtime_error{"AES cipher update failed"};

  const int final_ok = encrypt ? EVP_EncryptFinal_ex(ctx.get(), dst + out_len, &final_len)
                               : EVP_DecryptFinal_ex(ctx.get(), dst + out_len, &final_len);
  if (final_ok != 1)
    throw std::runtime_error{"AES cipher finalization failed"};

  out.resize(static_cast<std::size_t>(out_len + final_len));
  return out;
}

} // namespace

auto aes_cbc_encrypt(const std::span<const std::byte> data, const std::span<const std::byte> key,
                     const std::span<const std::byte> iv, const Padding padding) -> std::vector<std::byte> {
  return run(true, data, key, iv, padding);
}

auto aes_cbc_decrypt(const std::span<const std::byte> ciphertext, const std::span<const std::byte> key,
                     const std::span<const std::byte> iv, const Padding padding) -> std::vector<std::byte> {
  return run(false, ciphertext, key, iv, padding);
}

} // namespace kitzoo::util
