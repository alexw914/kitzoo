// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/base64.hpp
// Description: Declares Base64 encoding and decoding of byte sequences.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_BASE64_HPP
#define KITZOO_UTILITIES_BASE64_HPP

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kitzoo::util {

KZ_NODISCARD auto base64_encode(std::span<const std::byte> data) -> std::string;

KZ_NODISCARD auto base64_decode(std::string_view b64) -> std::optional<std::vector<std::byte>>;

} // namespace kitzoo::util

#endif // KITZOO_UTILITIES_BASE64_HPP
