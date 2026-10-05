// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/str.hpp
// Description: Declares string processing, numeric parsing, and hexadecimal conversion.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_STR_HPP
#define KITZOO_UTILITIES_STR_HPP

#include <kitzoo/core/macro.hpp>

#include <charconv>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kitzoo::util {

namespace detail {
inline constexpr std::string_view kAsciiWhitespace = " \t\n\r\f\v";
}

KZ_NODISCARD constexpr auto trim(std::string_view s) noexcept -> std::string_view {
  const auto start = s.find_first_not_of(detail::kAsciiWhitespace);
  if (start == std::string_view::npos)
    return {};
  const auto end = s.find_last_not_of(detail::kAsciiWhitespace);
  return s.substr(start, end - start + 1);
}

KZ_NODISCARD constexpr auto trim_left(std::string_view s) noexcept -> std::string_view {
  const auto start = s.find_first_not_of(detail::kAsciiWhitespace);
  if (start == std::string_view::npos)
    return {};
  return s.substr(start);
}

KZ_NODISCARD constexpr auto trim_right(std::string_view s) noexcept -> std::string_view {
  const auto end = s.find_last_not_of(detail::kAsciiWhitespace);
  if (end == std::string_view::npos)
    return {};
  return s.substr(0, end + 1);
}

// Views refer to the input. An empty delimiter yields the whole input; skip_empty
// drops empty fields such as those between adjacent delimiters.
KZ_NODISCARD auto split(std::string_view s, std::string_view delimiter, bool skip_empty = false)
    -> std::vector<std::string_view>;

KZ_NODISCARD auto join(std::span<const std::string_view> parts, std::string_view separator) -> std::string;

KZ_NODISCARD auto join(std::initializer_list<std::string_view> parts, std::string_view separator) -> std::string;

KZ_NODISCARD auto replace_all(std::string_view s, std::string_view from, std::string_view to) -> std::string;

KZ_NODISCARD auto to_lower(std::string_view s) -> std::string;

KZ_NODISCARD auto to_upper(std::string_view s) -> std::string;

KZ_NODISCARD auto hex_encode(std::span<const std::byte> data) -> std::string;

KZ_NODISCARD auto hex_decode(std::string_view hex) -> std::optional<std::vector<std::byte>>;

KZ_NODISCARD auto equals_ignore_case(std::string_view a, std::string_view b) noexcept -> bool;

template <std::integral T>
KZ_NODISCARD auto to_number(std::string_view s, int base = 10) noexcept -> std::optional<T> {
  s = trim(s);
  if (s.empty())
    return std::nullopt;

  T value{};
  const auto* const first = s.data();
  const auto* const last = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(first, last, value, base);

  if (ec == std::errc::invalid_argument) {
    return std::nullopt;
  }
  if (ec == std::errc::result_out_of_range) {
    return std::nullopt;
  }
  if (ptr != last) {
    return std::nullopt;
  }
  return value;
}

template <std::floating_point T>
KZ_NODISCARD auto to_number(std::string_view s) noexcept -> std::optional<T> {
  s = trim(s);
  if (s.empty())
    return std::nullopt;

  T value{};
  const auto* const first = s.data();
  const auto* const last = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(first, last, value);

  if (ec == std::errc::invalid_argument) {
    return std::nullopt;
  }
  if (ec == std::errc::result_out_of_range) {
    return std::nullopt;
  }
  if (ptr != last) {
    return std::nullopt;
  }
  return value;
}

template <std::integral T>
KZ_NODISCARD auto from_number(T value, int base = 10) -> std::string {
  char buf[std::numeric_limits<T>::digits10 + 4];
  const auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value, base);

  if (ec != std::errc{})
    return {};
  return {buf, static_cast<std::size_t>(ptr - buf)};
}

template <std::floating_point T>
KZ_NODISCARD auto from_number(T value) -> std::string {
  char buf[64];
  const auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
  if (ec != std::errc{})
    return {};
  return {buf, static_cast<std::size_t>(ptr - buf)};
}

} // namespace kitzoo::util

#endif // KITZOO_UTILITIES_STR_HPP
