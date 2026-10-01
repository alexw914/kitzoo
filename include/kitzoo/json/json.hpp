// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/json/json.hpp
// Description: Adds kitzoo parsing, serialization, and typed-access helpers
//              around nlohmann/json.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>
#include <kitzoo/filesystem/filesystem.hpp>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace kitzoo::json {

using Json = nlohmann::json;

KZ_NODISCARD inline auto parse(std::string_view text) -> Json {
    return Json::parse(text.begin(), text.end());
}

KZ_NODISCARD inline auto load_file(std::filesystem::path const& path) -> Json {
    return parse(fs::read_text(path));
}

KZ_NODISCARD inline auto serialize(Json const& value, int indent = 2) -> std::string {
    return value.dump(indent);
}

inline auto save_file(std::filesystem::path const& path, Json const& value, int indent = 2)
    -> void {
    auto text = serialize(value, indent);
    fs::atomic_write(path, std::span<char const>{text.data(), text.size()});
}

inline auto save_file(std::filesystem::path const& path, Json const& value, std::error_code& ec,
                      int indent = 2) -> void {
    auto text = serialize(value, indent);
    fs::atomic_write(path, std::span<char const>{text.data(), text.size()}, ec);
}

template <typename T>
KZ_NODISCARD auto get_or(Json const& j, std::string_view key, T fallback) -> T {
    if (!j.is_object())
        return fallback;
    auto const it = j.find(key);
    if (it == j.end())
        return fallback;
    try {
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

template <typename T>
KZ_NODISCARD auto get_optional(Json const& j, std::string_view key) -> std::optional<T> {
    if (!j.is_object())
        return std::nullopt;
    auto const it = j.find(key);
    if (it == j.end())
        return std::nullopt;
    try {
        return it->get<T>();
    } catch (...) {
        return std::nullopt;
    }
}

template <typename T>
KZ_NODISCARD auto get_at(Json const& j, std::string_view key) -> T {
    return j.at(std::string{key}).template get<T>();
}

}  // namespace kitzoo::json
