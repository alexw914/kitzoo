// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/random.hpp
// Description: Declares random number and string helpers backed by
//              standard-library random engines.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_RANDOM_HPP
#define KITZOO_UTILITIES_RANDOM_HPP

#include <kitzoo/core/macro.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>

namespace kitzoo::util {

KZ_NODISCARD inline auto thread_rng() -> std::mt19937_64& {
    thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

template <std::integral T>
KZ_NODISCARD auto random_int(T min, T max) -> T {
    std::uniform_int_distribution<T> dist{min, max};
    return dist(thread_rng());
}

template <std::floating_point T>
KZ_NODISCARD auto random_real(T min, T max) -> T {
    std::uniform_real_distribution<T> dist{min, max};
    return dist(thread_rng());
}

KZ_NODISCARD inline auto random_string(
    std::size_t len,
    std::string_view charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
    -> std::string {
    std::string out;
    out.reserve(len);
    std::uniform_int_distribution<std::size_t> dist{0, charset.size() - 1};
    for (std::size_t i = 0; i < len; ++i)
        out.push_back(charset[dist(thread_rng())]);
    return out;
}

template <typename Range>
auto shuffle(Range&& range) -> void {
    std::shuffle(std::begin(range), std::end(range), thread_rng());
}

}  // namespace kitzoo::util

#endif  // KITZOO_UTILITIES_RANDOM_HPP
