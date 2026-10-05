// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/filesys.hpp
// Description: Declares file, directory and path operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_FILESYS_HPP
#define KITZOO_OS_FILESYS_HPP

#include <kitzoo/core/macro.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kitzoo::os {

KZ_NODISCARD auto read_file(const std::filesystem::path& path, std::error_code& ec) -> std::string;
KZ_NODISCARD auto read_file(const std::filesystem::path& path) -> std::string;

auto write_file(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void;
auto write_file(const std::filesystem::path& path, std::string_view data) -> void;

auto atomic_write(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void;
auto atomic_write(const std::filesystem::path& path, std::string_view data) -> void;

KZ_NODISCARD auto temp_directory(std::error_code& ec) -> std::filesystem::path;
KZ_NODISCARD auto temp_directory() -> std::filesystem::path;

// Predicates follow symlinks except is_link; missing paths return false.
KZ_NODISCARD auto path_exists(const std::filesystem::path& path, std::error_code& ec) -> bool;
KZ_NODISCARD auto path_exists(const std::filesystem::path& path) -> bool;

KZ_NODISCARD auto is_regular(const std::filesystem::path& path, std::error_code& ec) -> bool;
KZ_NODISCARD auto is_regular(const std::filesystem::path& path) -> bool;

KZ_NODISCARD auto is_dir(const std::filesystem::path& path, std::error_code& ec) -> bool;
KZ_NODISCARD auto is_dir(const std::filesystem::path& path) -> bool;

KZ_NODISCARD auto is_link(const std::filesystem::path& path, std::error_code& ec) -> bool;
KZ_NODISCARD auto is_link(const std::filesystem::path& path) -> bool;

// parents=false creates one directory; true creates missing parents as well.
auto make_directory(const std::filesystem::path& path, std::error_code& ec, bool parents = false) -> bool;
auto make_directory(const std::filesystem::path& path, bool parents = false) -> bool;

// Default removal accepts files and empty directories; missing paths remove zero.
// Recursive removal does not follow directory symlinks.
auto remove_path(const std::filesystem::path& path, std::error_code& ec, bool recursive = false) -> std::uintmax_t;
auto remove_path(const std::filesystem::path& path, bool recursive = false) -> std::uintmax_t;

// Returns sorted paths. Recursive enumeration does not follow directory symlinks.
// Returns no partial list on an enumeration error.
KZ_NODISCARD auto list_directory(const std::filesystem::path& path, std::error_code& ec, bool recursive = false)
    -> std::vector<std::filesystem::path>;
KZ_NODISCARD auto list_directory(const std::filesystem::path& path, bool recursive = false)
    -> std::vector<std::filesystem::path>;

auto append_file(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void;
auto append_file(const std::filesystem::path& path, std::string_view data) -> void;

// Copies a regular file; overwrite is opt-in.
auto copy_file_to(const std::filesystem::path& source, const std::filesystem::path& destination, std::error_code& ec,
                  bool overwrite = false) -> bool;
auto copy_file_to(const std::filesystem::path& source, const std::filesystem::path& destination, bool overwrite = false)
    -> bool;

} // namespace kitzoo::os

#endif // KITZOO_OS_FILESYS_HPP
