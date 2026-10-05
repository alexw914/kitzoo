// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/utilities/str.cpp
// Description: Implements string processing and hexadecimal conversion.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/str.hpp>

namespace kitzoo::util {

auto join(const std::span<const std::string_view> parts, const std::string_view separator) -> std::string {
  if (parts.empty())
    return {};

  std::size_t total = 0;
  for (const auto& p : parts)
    total += p.size();
  total += separator.size() * (parts.size() - 1);

  std::string result;
  result.reserve(total);

  result.append(parts[0]);
  for (std::size_t i = 1; i < parts.size(); ++i) {
    result.append(separator);
    result.append(parts[i]);
  }
  return result;
}

auto join(const std::initializer_list<std::string_view> parts, const std::string_view separator) -> std::string {
  return join(std::span{parts.begin(), parts.size()}, separator);
}

auto replace_all(const std::string_view s, const std::string_view from, const std::string_view to) -> std::string {
  if (from.empty())
    return std::string{s};

  std::size_t count = 0;
  std::size_t search_pos = 0;
  while (true) {
    const auto pos = s.find(from, search_pos);
    if (pos == std::string_view::npos)
      break;
    ++count;
    search_pos = pos + from.size();
  }

  if (count == 0)
    return std::string{s};

  std::string result;
  if (to.size() > from.size()) {
    result.reserve(s.size() + count * (to.size() - from.size()));
  } else {
    result.reserve(s.size());
  }

  std::size_t last = 0;
  search_pos = 0;
  while (true) {
    const auto pos = s.find(from, search_pos);
    if (pos == std::string_view::npos) {
      result.append(s.substr(last));
      break;
    }
    result.append(s.substr(last, pos - last));
    result.append(to);
    search_pos = pos + from.size();
    last = search_pos;
  }
  return result;
}

auto to_lower(const std::string_view s) -> std::string {
  std::string result;
  result.reserve(s.size());
  for (const auto c : s) {
    if (static_cast<unsigned char>(c) >= 'A' && static_cast<unsigned char>(c) <= 'Z') {
      result.push_back(static_cast<char>(c + ('a' - 'A')));
    } else {
      result.push_back(c);
    }
  }
  return result;
}

auto to_upper(const std::string_view s) -> std::string {
  std::string result;
  result.reserve(s.size());
  for (const auto c : s) {
    if (static_cast<unsigned char>(c) >= 'a' && static_cast<unsigned char>(c) <= 'z') {
      result.push_back(static_cast<char>(c - ('a' - 'A')));
    } else {
      result.push_back(c);
    }
  }
  return result;
}

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
} // namespace

auto hex_encode(const std::span<const std::byte> data) -> std::string {
  std::string out;
  out.resize(data.size() * 2);
  for (std::size_t i = 0; i < data.size(); ++i) {
    const auto b = static_cast<unsigned char>(data[i]);
    out[i * 2] = kHexDigits[b >> 4];
    out[i * 2 + 1] = kHexDigits[b & 0x0F];
  }
  return out;
}

auto hex_decode(const std::string_view hex) -> std::optional<std::vector<std::byte>> {
  if (hex.size() % 2 != 0) {
    return std::nullopt;
  }
  std::vector<std::byte> out;
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const auto hi = hex_value(hex[i]);
    const auto lo = hex_value(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      return std::nullopt;
    }
    out.push_back(static_cast<std::byte>((hi << 4) | lo));
  }
  return out;
}

auto equals_ignore_case(const std::string_view a, const std::string_view b) noexcept -> bool {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto ca = static_cast<unsigned char>(a[i]);
    const auto cb = static_cast<unsigned char>(b[i]);
    const auto la = (ca >= 'A' && ca <= 'Z') ? ca + 32 : ca;
    const auto lb = (cb >= 'A' && cb <= 'Z') ? cb + 32 : cb;
    if (la != lb)
      return false;
  }
  return true;
}

} // namespace kitzoo::util
