// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/utilities/base64.cpp
// Description: Implements Base64 encoding and decoding of byte sequences.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/base64.hpp>

#include <cstdint>

namespace kitzoo::util {

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
} // namespace

auto base64_encode(const std::span<const std::byte> data) -> std::string {
  std::string out;
  out.reserve((data.size() + 2) / 3 * 4);

  std::size_t i = 0;
  for (; i + 3 <= data.size(); i += 3) {
    const auto n = (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                   static_cast<std::uint32_t>(data[i + 2]);
    out.push_back(kB64Alphabet[(n >> 18) & 63]);
    out.push_back(kB64Alphabet[(n >> 12) & 63]);
    out.push_back(kB64Alphabet[(n >> 6) & 63]);
    out.push_back(kB64Alphabet[n & 63]);
  }

  const auto rem = data.size() - i;
  if (rem == 1) {
    const auto n = static_cast<std::uint32_t>(data[i]) << 16;
    out.push_back(kB64Alphabet[(n >> 18) & 63]);
    out.push_back(kB64Alphabet[(n >> 12) & 63]);
    out += "==";
  } else if (rem == 2) {
    const auto n = (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8);
    out.push_back(kB64Alphabet[(n >> 18) & 63]);
    out.push_back(kB64Alphabet[(n >> 12) & 63]);
    out.push_back(kB64Alphabet[(n >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}

auto base64_decode(const std::string_view b64) -> std::optional<std::vector<std::byte>> {
  if (b64.size() % 4 != 0) {
    return std::nullopt;
  }

  std::vector<std::byte> out;
  out.reserve(b64.size() / 4 * 3);

  for (std::size_t i = 0; i < b64.size(); i += 4) {
    int v[4];
    int pad = 0;
    for (int j = 0; j < 4; ++j) {
      const char c = b64[i + static_cast<std::size_t>(j)];
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

    const auto n = (static_cast<std::uint32_t>(v[0]) << 18) | (static_cast<std::uint32_t>(v[1]) << 12) |
                   (static_cast<std::uint32_t>(v[2]) << 6) | static_cast<std::uint32_t>(v[3]);
    out.push_back(static_cast<std::byte>((n >> 16) & 0xFF));
    if (pad < 2)
      out.push_back(static_cast<std::byte>((n >> 8) & 0xFF));
    if (pad < 1)
      out.push_back(static_cast<std::byte>(n & 0xFF));
  }
  return out;
}

} // namespace kitzoo::util
