// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/filesystem.hpp
// Description: Declares file, directory, and path operations, including
//              error-code overloads and atomic file writing.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_FILESYSTEM_HPP
#define KITZOO_OS_FILESYSTEM_HPP

#include <kitzoo/core/macro.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kitzoo::os {

KZ_NODISCARD auto read_file(std::filesystem::path const& path, std::error_code& ec) -> std::string;
KZ_NODISCARD auto read_file(std::filesystem::path const& path) -> std::string;

KZ_NODISCARD auto read_text(std::filesystem::path const& path, std::error_code& ec) -> std::string;
KZ_NODISCARD auto read_text(std::filesystem::path const& path) -> std::string;

auto write_file(std::filesystem::path const& path, std::span<char const> data,
                std::error_code& ec) -> void;
auto write_file(std::filesystem::path const& path, std::span<char const> data) -> void;

auto write_text(std::filesystem::path const& path, std::string_view data,
                std::error_code& ec) -> void;
auto write_text(std::filesystem::path const& path, std::string_view data) -> void;

auto atomic_write(std::filesystem::path const& path, std::span<char const> data,
                  std::error_code& ec) -> void;
auto atomic_write(std::filesystem::path const& path, std::span<char const> data) -> void;

KZ_NODISCARD auto temp_directory(std::error_code& ec) -> std::filesystem::path;
KZ_NODISCARD auto temp_directory() -> std::filesystem::path;

KZ_NODISCARD auto current_path(std::error_code& ec) -> std::filesystem::path;
KZ_NODISCARD auto current_path() -> std::filesystem::path;
auto set_current_path(std::filesystem::path const& path, std::error_code& ec) -> void;
auto set_current_path(std::filesystem::path const& path) -> void;

auto create_directories(std::filesystem::path const& path, std::error_code& ec) -> bool;
auto create_directories(std::filesystem::path const& path) -> bool;

auto remove_all(std::filesystem::path const& path, std::error_code& ec) -> std::uintmax_t;
auto remove_all(std::filesystem::path const& path) -> std::uintmax_t;

KZ_NODISCARD auto list_directory(std::filesystem::path const& dir,
                                 std::error_code& ec) -> std::vector<std::filesystem::path>;
KZ_NODISCARD auto list_directory(std::filesystem::path const& dir)
    -> std::vector<std::filesystem::path>;

KZ_NODISCARD auto file_size(std::filesystem::path const& path,
                            std::error_code& ec) -> std::uintmax_t;
KZ_NODISCARD auto file_size(std::filesystem::path const& path) -> std::uintmax_t;

}  // namespace kitzoo::os

#endif  // KITZOO_OS_FILESYSTEM_HPP
