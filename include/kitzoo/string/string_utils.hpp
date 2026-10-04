// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/string/string_utils.hpp
// Description: Declares string-view utilities for trimming, joining,
//              replacement, and ASCII case-insensitive operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_STRING_STRING_UTILS_HPP
#define KITZOO_STRING_STRING_UTILS_HPP

#include <kitzoo/core/macro.hpp>

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace kitzoo::str {

namespace detail {
inline constexpr std::string_view kAsciiWhitespace = " \t\n\r\f\v";
}

KZ_NODISCARD constexpr auto trim(std::string_view s) noexcept -> std::string_view {
    auto const start = s.find_first_not_of(detail::kAsciiWhitespace);
    if (start == std::string_view::npos)
        return {};
    auto const end = s.find_last_not_of(detail::kAsciiWhitespace);
    return s.substr(start, end - start + 1);
}

KZ_NODISCARD constexpr auto trim_left(std::string_view s) noexcept -> std::string_view {
    auto const start = s.find_first_not_of(detail::kAsciiWhitespace);
    if (start == std::string_view::npos)
        return {};
    return s.substr(start);
}

KZ_NODISCARD constexpr auto trim_right(std::string_view s) noexcept -> std::string_view {
    auto const end = s.find_last_not_of(detail::kAsciiWhitespace);
    if (end == std::string_view::npos)
        return {};
    return s.substr(0, end + 1);
}

KZ_NODISCARD auto join(std::span<std::string_view const> parts,
                       std::string_view separator) -> std::string;

KZ_NODISCARD auto join(std::initializer_list<std::string_view> parts,
                       std::string_view separator) -> std::string;

KZ_NODISCARD auto replace_all(std::string_view s, std::string_view from,
                              std::string_view to) -> std::string;

KZ_NODISCARD auto to_lower(std::string_view s) -> std::string;
KZ_NODISCARD auto to_upper(std::string_view s) -> std::string;

}  // namespace kitzoo::str

#endif  // KITZOO_STRING_STRING_UTILS_HPP
