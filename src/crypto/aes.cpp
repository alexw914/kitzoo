// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/crypto/aes.cpp
// Description: Implements AES-CBC encryption and decryption through OpenSSL
//              EVP, including padding validation and error handling.
// -----------------------------------------------------------------------------

#include <kitzoo/crypto/aes.hpp>

#include <memory>
#include <openssl/evp.h>
#include <stdexcept>

namespace kitzoo::crypto {

namespace {

using EvpCtx = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

auto cipher_for(std::size_t const key_size) -> EVP_CIPHER const* {
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

auto run(bool const encrypt, std::span<std::byte const> const input, std::span<std::byte const> const key,
         std::span<std::byte const> const iv, Padding const padding) -> std::vector<std::byte> {
  EVP_CIPHER const* const cipher = cipher_for(key.size());
  if (cipher == nullptr) {
    throw std::invalid_argument{"AES key must be 16, 24, or 32 bytes"};
  }
  auto const iv_len = static_cast<std::size_t>(EVP_CIPHER_iv_length(cipher));
  if (iv.size() != iv_len) {
    throw std::invalid_argument{"AES-CBC IV must be 16 bytes"};
  }
  auto const block_size = static_cast<std::size_t>(EVP_CIPHER_block_size(cipher));
  if (padding == Padding::None && input.size() % block_size != 0) {
    throw std::invalid_argument{"AES-CBC input must be block-aligned without padding"};
  }

  EvpCtx ctx{EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free};
  if (!ctx)
    throw std::runtime_error{"EVP_CIPHER_CTX_new failed"};

  auto const* const k = reinterpret_cast<unsigned char const*>(key.data());
  auto const* const v = reinterpret_cast<unsigned char const*>(iv.data());

  int const init_ok = encrypt ? EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, k, v)
                              : EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, k, v);
  if (init_ok != 1)
    throw std::runtime_error{"AES cipher initialization failed"};

  EVP_CIPHER_CTX_set_padding(ctx.get(), padding == Padding::Pkcs7 ? 1 : 0);

  std::vector<std::byte> out(input.size() + block_size);
  int out_len = 0;
  int final_len = 0;

  auto const* const in = reinterpret_cast<unsigned char const*>(input.data());
  auto* const dst = reinterpret_cast<unsigned char*>(out.data());

  int const update_ok = encrypt ? EVP_EncryptUpdate(ctx.get(), dst, &out_len, in, static_cast<int>(input.size()))
                                : EVP_DecryptUpdate(ctx.get(), dst, &out_len, in, static_cast<int>(input.size()));
  if (update_ok != 1)
    throw std::runtime_error{"AES cipher update failed"};

  int const final_ok = encrypt ? EVP_EncryptFinal_ex(ctx.get(), dst + out_len, &final_len)
                               : EVP_DecryptFinal_ex(ctx.get(), dst + out_len, &final_len);
  if (final_ok != 1)
    throw std::runtime_error{"AES cipher finalization failed"};

  out.resize(static_cast<std::size_t>(out_len + final_len));
  return out;
}

} // namespace

auto aes_cbc_encrypt(std::span<std::byte const> const data, std::span<std::byte const> const key,
                     std::span<std::byte const> const iv, Padding const padding) -> std::vector<std::byte> {
  return run(true, data, key, iv, padding);
}

auto aes_cbc_decrypt(std::span<std::byte const> const ciphertext, std::span<std::byte const> const key,
                     std::span<std::byte const> const iv, Padding const padding) -> std::vector<std::byte> {
  return run(false, ciphertext, key, iv, padding);
}

} // namespace kitzoo::crypto
