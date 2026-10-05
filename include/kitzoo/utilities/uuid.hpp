// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/uuid.hpp
// Description: Declares the UUID value type, generation helpers, formatting,
//              and parsing operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_UUID_HPP
#define KITZOO_UTILITIES_UUID_HPP

#include <kitzoo/core/macro.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace kitzoo::util {

class Uuid {
public:
  using Bytes = std::array<std::uint8_t, 16>;

  constexpr Uuid() noexcept = default;

  explicit constexpr Uuid(Bytes bytes) noexcept : bytes_{bytes} {}

  KZ_NODISCARD static auto random() -> Uuid {
    std::random_device source;
    std::uniform_int_distribution<unsigned> byte{0, 255};
    Bytes data{};
    for (auto& value : data)
      value = static_cast<std::uint8_t>(byte(source));
    data[6] = static_cast<std::uint8_t>((data[6] & 0x0fU) | 0x40U);
    data[8] = static_cast<std::uint8_t>((data[8] & 0x3fU) | 0x80U);
    return Uuid{data};
  }

  KZ_NODISCARD static auto parse(std::string_view text) -> std::optional<Uuid> {
    if (text.size() != 36)
      return std::nullopt;

    Bytes data{};
    std::size_t byte_index = 0;
    int high_nibble = -1;
    for (std::size_t i = 0; i < text.size(); ++i) {
      if (i == 8 || i == 13 || i == 18 || i == 23) {
        if (text[i] != '-')
          return std::nullopt;
        continue;
      }
      auto const nibble = hex_value(text[i]);
      if (nibble < 0)
        return std::nullopt;
      if (high_nibble < 0) {
        high_nibble = nibble;
      } else {
        data[byte_index++] = static_cast<std::uint8_t>((high_nibble << 4) | nibble);
        high_nibble = -1;
      }
    }
    return Uuid{data};
  }

  KZ_NODISCARD constexpr auto bytes() const noexcept -> Bytes const& { return bytes_; }

  KZ_NODISCARD constexpr auto is_nil() const noexcept -> bool {
    for (auto byte : bytes_)
      if (byte != 0)
        return false;
    return true;
  }

  KZ_NODISCARD auto to_string() const -> std::string {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(36);
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
      if (i == 4 || i == 6 || i == 8 || i == 10)
        text.push_back('-');
      text.push_back(digits[bytes_[i] >> 4]);
      text.push_back(digits[bytes_[i] & 0x0fU]);
    }
    return text;
  }

  friend constexpr auto operator==(Uuid const&, Uuid const&) noexcept -> bool = default;

private:
  static constexpr auto hex_value(char c) noexcept -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }

  Bytes bytes_{};
};

} // namespace kitzoo::util

template <>
struct std::hash<kitzoo::util::Uuid> {
  KZ_NODISCARD auto operator()(kitzoo::util::Uuid const& uuid) const noexcept -> std::size_t {
    std::size_t value = 0;
    for (auto byte : uuid.bytes())
      value = value * 31 + byte;
    return value;
  }
};

#endif // KITZOO_UTILITIES_UUID_HPP
