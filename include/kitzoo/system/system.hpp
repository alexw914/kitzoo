// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/system/system.hpp
// Description: Declares operating-system queries for environment, host,
//              process, memory, user, and stack-trace information.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kitzoo::sys {

KZ_NODISCARD auto get_env(std::string_view name) -> std::optional<std::string>;

KZ_NODISCARD auto hostname() -> std::string;

KZ_NODISCARD auto cpu_count() noexcept -> unsigned int;

KZ_NODISCARD auto current_pid() noexcept -> long;

KZ_NODISCARD auto page_size() noexcept -> std::size_t;

KZ_NODISCARD auto total_memory() noexcept -> std::uint64_t;

KZ_NODISCARD auto username() -> std::string;

KZ_NODISCARD auto home_dir() -> std::string;

KZ_NODISCARD auto stacktrace(int max_frames = 64) -> std::vector<std::string>;

}  // namespace kitzoo::sys
