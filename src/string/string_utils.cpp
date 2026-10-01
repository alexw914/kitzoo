// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/string/string_utils.cpp
// Description: Implements string-view trimming, joining, replacement, and ASCII
//              case conversion and comparison.
// -----------------------------------------------------------------------------

#include <kitzoo/string/string_utils.hpp>

namespace kitzoo::str {

auto join(std::span<std::string_view const> const parts, std::string_view const separator)
    -> std::string {
    if (parts.empty())
        return {};

    std::size_t total = 0;
    for (auto const& p : parts)
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

auto join(std::initializer_list<std::string_view> const parts, std::string_view const separator)
    -> std::string {
    return join(std::span{parts.begin(), parts.size()}, separator);
}

auto replace_all(std::string_view const s, std::string_view const from, std::string_view const to)
    -> std::string {
    if (from.empty())
        return std::string{s};

    std::size_t count = 0;
    std::size_t search_pos = 0;
    while (true) {
        auto const pos = s.find(from, search_pos);
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
        auto const pos = s.find(from, search_pos);
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

auto to_lower(std::string_view const s) -> std::string {
    std::string result;
    result.reserve(s.size());
    for (auto const c : s) {
        if (static_cast<unsigned char>(c) >= 'A' && static_cast<unsigned char>(c) <= 'Z') {
            result.push_back(static_cast<char>(c + ('a' - 'A')));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

auto to_upper(std::string_view const s) -> std::string {
    std::string result;
    result.reserve(s.size());
    for (auto const c : s) {
        if (static_cast<unsigned char>(c) >= 'a' && static_cast<unsigned char>(c) <= 'z') {
            result.push_back(static_cast<char>(c - ('a' - 'A')));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

}  // namespace kitzoo::str
