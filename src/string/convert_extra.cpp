// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/string/convert_extra.cpp
// Description: Implements hexadecimal, Base64, and byte-buffer conversions
//              declared by the string module.
// -----------------------------------------------------------------------------

#include <kitzoo/string/convert_extra.hpp>

#include <cstdint>

namespace kitzoo::str {

namespace {
constexpr char kHexDigits[] = "0123456789abcdef";

auto hex_value(char c) noexcept -> int {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
}  // namespace

auto hex_encode(std::span<std::byte const> const data) -> std::string {
    std::string out;
    out.resize(data.size() * 2);
    for (std::size_t i = 0; i < data.size(); ++i) {
        auto const b = static_cast<unsigned char>(data[i]);
        out[i * 2] = kHexDigits[b >> 4];
        out[i * 2 + 1] = kHexDigits[b & 0x0F];
    }
    return out;
}

auto hex_decode(std::string_view const hex) -> std::optional<std::vector<std::byte>> {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::byte> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        auto const hi = hex_value(hex[i]);
        auto const lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        out.push_back(static_cast<std::byte>((hi << 4) | lo));
    }
    return out;
}

namespace {
constexpr char kB64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

auto b64_value(char c) noexcept -> int {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}
}  // namespace

auto base64_encode(std::span<std::byte const> const data) -> std::string {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);

    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        auto const n = (static_cast<std::uint32_t>(data[i]) << 16) |
                       (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                       static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kB64Alphabet[(n >> 18) & 63]);
        out.push_back(kB64Alphabet[(n >> 12) & 63]);
        out.push_back(kB64Alphabet[(n >> 6) & 63]);
        out.push_back(kB64Alphabet[n & 63]);
    }

    auto const rem = data.size() - i;
    if (rem == 1) {
        auto const n = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kB64Alphabet[(n >> 18) & 63]);
        out.push_back(kB64Alphabet[(n >> 12) & 63]);
        out += "==";
    } else if (rem == 2) {
        auto const n = (static_cast<std::uint32_t>(data[i]) << 16) |
                       (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kB64Alphabet[(n >> 18) & 63]);
        out.push_back(kB64Alphabet[(n >> 12) & 63]);
        out.push_back(kB64Alphabet[(n >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

auto base64_decode(std::string_view const b64) -> std::optional<std::vector<std::byte>> {
    if (b64.size() % 4 != 0) {
        return std::nullopt;
    }

    std::vector<std::byte> out;
    out.reserve(b64.size() / 4 * 3);

    for (std::size_t i = 0; i < b64.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int j = 0; j < 4; ++j) {
            char const c = b64[i + static_cast<std::size_t>(j)];
            if (c == '=') {
                if (i + 4 != b64.size() || j < 2) {
                    return std::nullopt;
                }
                ++pad;
                v[j] = 0;
            } else {
                v[j] = b64_value(c);
                if (v[j] < 0) {
                    return std::nullopt;
                }
            }
        }
        if (pad > 2) {
            return std::nullopt;
        }

        auto const n = (static_cast<std::uint32_t>(v[0]) << 18) |
                       (static_cast<std::uint32_t>(v[1]) << 12) |
                       (static_cast<std::uint32_t>(v[2]) << 6) | static_cast<std::uint32_t>(v[3]);
        out.push_back(static_cast<std::byte>((n >> 16) & 0xFF));
        if (pad < 2)
            out.push_back(static_cast<std::byte>((n >> 8) & 0xFF));
        if (pad < 1)
            out.push_back(static_cast<std::byte>(n & 0xFF));
    }
    return out;
}

auto equals_ignore_case(std::string_view const a, std::string_view const b) noexcept -> bool {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto const ca = static_cast<unsigned char>(a[i]);
        auto const cb = static_cast<unsigned char>(b[i]);
        auto const la = (ca >= 'A' && ca <= 'Z') ? ca + 32 : ca;
        auto const lb = (cb >= 'A' && cb <= 'Z') ? cb + 32 : cb;
        if (la != lb)
            return false;
    }
    return true;
}

}  // namespace kitzoo::str
