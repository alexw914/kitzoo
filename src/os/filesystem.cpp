// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/os/filesystem.cpp
// Description: Preserves legacy filesystem functions by forwarding to FsAdaptor.
// -----------------------------------------------------------------------------

#include <kitzoo/os/filesystem.hpp>

namespace kitzoo::os {

auto read_file(std::filesystem::path const& path, std::error_code& ec) -> std::string {
    return FsAdaptor::instance().read_file(path, ec);
}

auto read_file(std::filesystem::path const& path) -> std::string {
    return FsAdaptor::instance().read_file(path);
}

auto read_text(std::filesystem::path const& path, std::error_code& ec) -> std::string {
    return FsAdaptor::instance().read_text(path, ec);
}

auto read_text(std::filesystem::path const& path) -> std::string {
    return FsAdaptor::instance().read_text(path);
}

auto write_file(std::filesystem::path const& path, std::span<char const> data,
                std::error_code& ec) -> void {
    FsAdaptor::instance().write_file(path, data, ec);
}

auto write_file(std::filesystem::path const& path, std::span<char const> data) -> void {
    FsAdaptor::instance().write_file(path, data);
}

auto write_text(std::filesystem::path const& path, std::string_view data,
                std::error_code& ec) -> void {
    FsAdaptor::instance().write_text(path, data, ec);
}

auto write_text(std::filesystem::path const& path, std::string_view data) -> void {
    FsAdaptor::instance().write_text(path, data);
}

auto atomic_write(std::filesystem::path const& path, std::span<char const> data,
                  std::error_code& ec) -> void {
    FsAdaptor::instance().atomic_write(path, data, ec);
}

auto atomic_write(std::filesystem::path const& path, std::span<char const> data) -> void {
    FsAdaptor::instance().atomic_write(path, data);
}

auto temp_directory(std::error_code& ec) -> std::filesystem::path {
    return FsAdaptor::instance().temp_directory(ec);
}

auto temp_directory() -> std::filesystem::path {
    return FsAdaptor::instance().temp_directory();
}

auto current_path(std::error_code& ec) -> std::filesystem::path {
    return FsAdaptor::instance().current_path(ec);
}

auto current_path() -> std::filesystem::path {
    return FsAdaptor::instance().current_path();
}

auto set_current_path(std::filesystem::path const& path, std::error_code& ec) -> void {
    FsAdaptor::instance().set_current_path(path, ec);
}

auto set_current_path(std::filesystem::path const& path) -> void {
    FsAdaptor::instance().set_current_path(path);
}

auto create_directories(std::filesystem::path const& path, std::error_code& ec) -> bool {
    return FsAdaptor::instance().create_directories(path, ec);
}

auto create_directories(std::filesystem::path const& path) -> bool {
    return FsAdaptor::instance().create_directories(path);
}

auto remove_all(std::filesystem::path const& path, std::error_code& ec) -> std::uintmax_t {
    return FsAdaptor::instance().remove_all(path, ec);
}

auto remove_all(std::filesystem::path const& path) -> std::uintmax_t {
    return FsAdaptor::instance().remove_all(path);
}

auto list_directory(std::filesystem::path const& dir,
                    std::error_code& ec) -> std::vector<std::filesystem::path> {
    return FsAdaptor::instance().list_directory(dir, ec);
}

auto list_directory(std::filesystem::path const& dir) -> std::vector<std::filesystem::path> {
    return FsAdaptor::instance().list_directory(dir);
}

auto file_size(std::filesystem::path const& path, std::error_code& ec) -> std::uintmax_t {
    return FsAdaptor::instance().file_size(path, ec);
}

auto file_size(std::filesystem::path const& path) -> std::uintmax_t {
    return FsAdaptor::instance().file_size(path);
}

}  // namespace kitzoo::os
