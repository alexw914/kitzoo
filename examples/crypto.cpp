#include <kitzoo/crypto.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

int main() {
    // CBC is unauthenticated; use an AEAD mode for new protocols.
    constexpr std::array key{std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
                             std::byte{0x04}, std::byte{0x05}, std::byte{0x06}, std::byte{0x07},
                             std::byte{0x08}, std::byte{0x09}, std::byte{0x0a}, std::byte{0x0b},
                             std::byte{0x0c}, std::byte{0x0d}, std::byte{0x0e}, std::byte{0x0f}};
    constexpr std::array<std::byte, 16> iv{};
    constexpr std::string_view text = "example payload";
    auto const input = std::as_bytes(std::span{text.data(), text.size()});
    auto encrypted = kitzoo::crypto::aes_cbc_encrypt(input, key, iv);
    auto decrypted = kitzoo::crypto::aes_cbc_decrypt(encrypted, key, iv);
    if (decrypted.size() != text.size())
        return 1;
    std::printf("AES-CBC round-trip: %zu bytes\n", decrypted.size());
}
