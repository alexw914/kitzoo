// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/string/convert_extra.hpp
// Description: Declares hexadecimal, Base64, and byte-buffer conversion
//              helpers.
// -----------------------------------------------------------------------------

#ifndef KITZOO_STRING_CONVERT_EXTRA_HPP
#define KITZOO_STRING_CONVERT_EXTRA_HPP

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kitzoo::str {

KZ_NODISCARD auto hex_encode(std::span<std::byte const> data) -> std::string;
KZ_NODISCARD auto hex_decode(std::string_view hex) -> std::optional<std::vector<std::byte>>;

KZ_NODISCARD auto base64_encode(std::span<std::byte const> data) -> std::string;
KZ_NODISCARD auto base64_decode(std::string_view b64) -> std::optional<std::vector<std::byte>>;

KZ_NODISCARD auto equals_ignore_case(std::string_view a, std::string_view b) noexcept -> bool;

} // namespace kitzoo::str

#endif // KITZOO_STRING_CONVERT_EXTRA_HPP
