// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/fsadaptor.hpp
// Description: Declares singleton file, directory and path operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_FSADAPTOR_HPP
#define KITZOO_OS_FSADAPTOR_HPP

#include <kitzoo/utilities/singleton.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kitzoo::os {

class FsAdaptor : public util::Singleton<FsAdaptor> {
public:
    auto read_file(std::filesystem::path const& path, std::error_code& ec) const -> std::string;
    auto read_file(std::filesystem::path const& path) const -> std::string;

    auto read_text(std::filesystem::path const& path, std::error_code& ec) const -> std::string;
    auto read_text(std::filesystem::path const& path) const -> std::string;

    auto write_file(std::filesystem::path const& path, std::span<char const> data,
                    std::error_code& ec) const -> void;
    auto write_file(std::filesystem::path const& path, std::span<char const> data) const -> void;

    auto write_text(std::filesystem::path const& path, std::string_view data,
                    std::error_code& ec) const -> void;
    auto write_text(std::filesystem::path const& path, std::string_view data) const -> void;

    auto atomic_write(std::filesystem::path const& path, std::span<char const> data,
                      std::error_code& ec) const -> void;
    auto atomic_write(std::filesystem::path const& path, std::span<char const> data) const -> void;

    auto temp_directory(std::error_code& ec) const -> std::filesystem::path;
    auto temp_directory() const -> std::filesystem::path;

    auto current_path(std::error_code& ec) const -> std::filesystem::path;
    auto current_path() const -> std::filesystem::path;
    auto set_current_path(std::filesystem::path const& path, std::error_code& ec) const -> void;
    auto set_current_path(std::filesystem::path const& path) const -> void;

    auto create_directories(std::filesystem::path const& path, std::error_code& ec) const -> bool;
    auto create_directories(std::filesystem::path const& path) const -> bool;

    auto remove_all(std::filesystem::path const& path, std::error_code& ec) const -> std::uintmax_t;
    auto remove_all(std::filesystem::path const& path) const -> std::uintmax_t;

    auto list_directory(std::filesystem::path const& dir,
                        std::error_code& ec) const -> std::vector<std::filesystem::path>;
    auto list_directory(std::filesystem::path const& dir) const
        -> std::vector<std::filesystem::path>;

    auto file_size(std::filesystem::path const& path, std::error_code& ec) const -> std::uintmax_t;
    auto file_size(std::filesystem::path const& path) const -> std::uintmax_t;

    // Predicates follow symlinks except is_symlink; missing paths return false.
    auto exists(const std::filesystem::path& path, std::error_code& ec) const -> bool;
    auto exists(const std::filesystem::path& path) const -> bool;

    auto is_file(const std::filesystem::path& path, std::error_code& ec) const -> bool;
    auto is_file(const std::filesystem::path& path) const -> bool;

    auto is_folder(const std::filesystem::path& path, std::error_code& ec) const -> bool;
    auto is_folder(const std::filesystem::path& path) const -> bool;

    auto is_symlink(const std::filesystem::path& path, std::error_code& ec) const -> bool;
    auto is_symlink(const std::filesystem::path& path) const -> bool;

    auto is_empty(const std::filesystem::path& path, std::error_code& ec) const -> bool;
    auto is_empty(const std::filesystem::path& path) const -> bool;

    // parents=false creates one directory; true creates missing parents as well.
    auto mkdir(const std::filesystem::path& path, std::error_code& ec,
               bool parents = false) const -> bool;
    auto mkdir(const std::filesystem::path& path, bool parents = false) const -> bool;

    // Default removal accepts files and empty directories; missing paths remove zero.
    // Recursive removal does not follow directory symlinks.
    auto rm(const std::filesystem::path& path, std::error_code& ec,
            bool recursive = false) const -> std::uintmax_t;
    auto rm(const std::filesystem::path& path, bool recursive = false) const -> std::uintmax_t;

    // Returns sorted paths. Recursive enumeration does not follow directory symlinks.
    // Returns no partial list on an enumeration error.
    auto listdir(const std::filesystem::path& path, std::error_code& ec,
                 bool recursive = false) const -> std::vector<std::filesystem::path>;
    auto listdir(const std::filesystem::path& path,
                 bool recursive = false) const -> std::vector<std::filesystem::path>;

    auto append_text(const std::filesystem::path& path, std::string_view data,
                     std::error_code& ec) const -> void;
    auto append_text(const std::filesystem::path& path, std::string_view data) const -> void;

    // Copies a regular file; overwrite is opt-in.
    auto copy_file(const std::filesystem::path& source, const std::filesystem::path& destination,
                   std::error_code& ec, bool overwrite = false) const -> bool;
    auto copy_file(const std::filesystem::path& source, const std::filesystem::path& destination,
                   bool overwrite = false) const -> bool;

    auto rename(const std::filesystem::path& source, const std::filesystem::path& destination,
                std::error_code& ec) const -> void;
    auto rename(const std::filesystem::path& source,
                const std::filesystem::path& destination) const -> void;

    auto absolute(const std::filesystem::path& path,
                  std::error_code& ec) const -> std::filesystem::path;
    auto absolute(const std::filesystem::path& path) const -> std::filesystem::path;

    auto canonical(const std::filesystem::path& path,
                   std::error_code& ec) const -> std::filesystem::path;
    auto canonical(const std::filesystem::path& path) const -> std::filesystem::path;

private:
    friend class util::Singleton<FsAdaptor>;
    FsAdaptor() = default;
};

}  // namespace kitzoo::os

#endif  // KITZOO_OS_FSADAPTOR_HPP
