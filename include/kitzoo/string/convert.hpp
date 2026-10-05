// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/string/convert.hpp
// Description: Declares string-to-number and number-to-string conversion
//              helpers with explicit parse results.
// -----------------------------------------------------------------------------

#ifndef KITZOO_STRING_CONVERT_HPP
#define KITZOO_STRING_CONVERT_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/string/string_utils.hpp>

#include <charconv>
#include <concepts>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace kitzoo::str {

template <std::integral T>
KZ_NODISCARD auto to_number(std::string_view s, int base = 10) noexcept -> std::optional<T> {
  s = trim(s);
  if (s.empty())
    return std::nullopt;

  T value{};
  auto const* const first = s.data();
  auto const* const last = s.data() + s.size();
  auto const [ptr, ec] = std::from_chars(first, last, value, base);

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
  auto const* const first = s.data();
  auto const* const last = s.data() + s.size();
  auto const [ptr, ec] = std::from_chars(first, last, value);

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
  auto const [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value, base);

  if (ec != std::errc{})
    return {};
  return {buf, static_cast<std::size_t>(ptr - buf)};
}

template <std::floating_point T>
KZ_NODISCARD auto from_number(T value) -> std::string {
  char buf[64];
  auto const [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
  if (ec != std::errc{})
    return {};
  return {buf, static_cast<std::size_t>(ptr - buf)};
}

} // namespace kitzoo::str

#endif // KITZOO_STRING_CONVERT_HPP
