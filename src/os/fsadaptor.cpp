// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/os/fsadaptor.cpp
// Description: Implements filesystem reading, writing, atomic replacement,
//              temporary directories, and path enumeration.
// -----------------------------------------------------------------------------

#include <kitzoo/os/fsadaptor.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <utility>

namespace kitzoo::os {

namespace {

auto open_input(std::filesystem::path const& path, std::error_code& ec) -> std::ifstream {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
    }
    return file;
}

auto open_output(std::filesystem::path const& path, std::error_code& ec) -> std::ofstream {
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file) {
        ec = std::make_error_code(std::errc::io_error);
    }
    return file;
}

auto throw_if_error(std::error_code const& ec, std::filesystem::path const& path,
                    char const* op) -> void {
    if (ec)
        throw std::filesystem::filesystem_error{op, path, ec};
}

auto read_file_impl(std::filesystem::path const& path, std::error_code& ec) -> std::string {
    ec.clear();
    auto file = open_input(path, ec);
    if (ec)
        return {};

    auto const size = file.tellg();
    if (size < 0) {
        ec = std::make_error_code(std::errc::io_error);
        return {};
    }

    file.seekg(0, std::ios::beg);
    std::string result;
    if (size > 0) {
        result.resize(static_cast<std::size_t>(size));
        file.read(result.data(), size);
        if (!file) {
            ec = std::make_error_code(std::errc::io_error);
            return {};
        }
    }
    return result;
}

auto write_file_impl(std::filesystem::path const& path, std::span<char const> data,
                     std::error_code& ec) -> void {
    ec.clear();
    auto file = open_output(path, ec);
    if (ec)
        return;
    if (!std::in_range<std::streamsize>(data.size())) {
        ec = std::make_error_code(std::errc::file_too_large);
        return;
    }
    if (!data.empty())
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.close();
    if (!file) {
        ec = std::make_error_code(std::errc::io_error);
    }
}

auto random_suffix() -> std::string {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;
    return std::to_string(dist(rng));
}

auto query_status(const std::filesystem::path& path, std::error_code& ec,
                  bool follow = true) -> std::filesystem::file_status {
    ec.clear();
    auto status =
        follow ? std::filesystem::status(path, ec) : std::filesystem::symlink_status(path, ec);
    if (status.type() == std::filesystem::file_type::not_found)
        ec.clear();
    return status;
}

template <typename Iterator>
auto collect_paths(Iterator iterator, std::error_code& ec) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> paths;
    const Iterator end;
    while (!ec && iterator != end) {
        paths.push_back(iterator->path());
        iterator.increment(ec);
    }
    if (ec)
        return {};
    std::sort(paths.begin(), paths.end());
    return paths;
}

}  // namespace

auto FsAdaptor::read_file(std::filesystem::path const& path,
                          std::error_code& ec) const -> std::string {
    return read_file_impl(path, ec);
}

auto FsAdaptor::read_file(std::filesystem::path const& path) const -> std::string {
    std::error_code ec;
    auto result = read_file_impl(path, ec);
    throw_if_error(ec, path, "read_file");
    return result;
}

auto FsAdaptor::read_text(std::filesystem::path const& path,
                          std::error_code& ec) const -> std::string {
    return read_file_impl(path, ec);
}

auto FsAdaptor::read_text(std::filesystem::path const& path) const -> std::string {
    std::error_code ec;
    auto result = read_file_impl(path, ec);
    throw_if_error(ec, path, "read_text");
    return result;
}

auto FsAdaptor::write_file(std::filesystem::path const& path, std::span<char const> data,
                           std::error_code& ec) const -> void {
    write_file_impl(path, data, ec);
}

auto FsAdaptor::write_file(std::filesystem::path const& path,
                           std::span<char const> data) const -> void {
    std::error_code ec;
    write_file_impl(path, data, ec);
    throw_if_error(ec, path, "write_file");
}

auto FsAdaptor::write_text(std::filesystem::path const& path, std::string_view data,
                           std::error_code& ec) const -> void {
    std::span const bytes{data.data(), data.size()};
    write_file_impl(path, bytes, ec);
}

auto FsAdaptor::write_text(std::filesystem::path const& path, std::string_view data) const -> void {
    std::error_code ec;
    write_text(path, data, ec);
    throw_if_error(ec, path, "write_text");
}

auto FsAdaptor::atomic_write(std::filesystem::path const& path, std::span<char const> data,
                             std::error_code& ec) const -> void {
    ec.clear();

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec)
            return;
    }

    auto tmp = path;
    tmp += ".tmp." + random_suffix();
    write_file_impl(tmp, data, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp, cleanup_ec);
        return;
    }

    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp, cleanup_ec);
    }
}

auto FsAdaptor::atomic_write(std::filesystem::path const& path,
                             std::span<char const> data) const -> void {
    std::error_code ec;
    atomic_write(path, data, ec);
    throw_if_error(ec, path, "atomic_write");
}

auto FsAdaptor::temp_directory(std::error_code& ec) const -> std::filesystem::path {
    ec.clear();
    auto base = std::filesystem::temp_directory_path(ec);
    if (ec)
        return {};

    auto candidate = base / ("kitzoo_" + random_suffix());
    while (!std::filesystem::create_directory(candidate, ec)) {
        if (ec)
            return {};
        candidate = base / ("kitzoo_" + random_suffix());
    }
    return candidate;
}

auto FsAdaptor::temp_directory() const -> std::filesystem::path {
    std::error_code ec;
    auto result = temp_directory(ec);
    throw_if_error(ec, "", "temp_directory");
    return result;
}

auto FsAdaptor::current_path(std::error_code& ec) const -> std::filesystem::path {
    return std::filesystem::current_path(ec);
}

auto FsAdaptor::current_path() const -> std::filesystem::path {
    return std::filesystem::current_path();
}

auto FsAdaptor::set_current_path(std::filesystem::path const& path,
                                 std::error_code& ec) const -> void {
    std::filesystem::current_path(path, ec);
}

auto FsAdaptor::set_current_path(std::filesystem::path const& path) const -> void {
    std::filesystem::current_path(path);
}

auto FsAdaptor::create_directories(std::filesystem::path const& path,
                                   std::error_code& ec) const -> bool {
    return std::filesystem::create_directories(path, ec);
}

auto FsAdaptor::create_directories(std::filesystem::path const& path) const -> bool {
    return std::filesystem::create_directories(path);
}

auto FsAdaptor::remove_all(std::filesystem::path const& path,
                           std::error_code& ec) const -> std::uintmax_t {
    return std::filesystem::remove_all(path, ec);
}

auto FsAdaptor::remove_all(std::filesystem::path const& path) const -> std::uintmax_t {
    std::error_code ec;
    auto count = std::filesystem::remove_all(path, ec);
    throw_if_error(ec, path, "remove_all");
    return count;
}

auto FsAdaptor::list_directory(const std::filesystem::path& dir,
                               std::error_code& ec) const -> std::vector<std::filesystem::path> {
    return listdir(dir, ec);
}

auto FsAdaptor::list_directory(const std::filesystem::path& dir) const
    -> std::vector<std::filesystem::path> {
    return listdir(dir);
}

auto FsAdaptor::file_size(std::filesystem::path const& path,
                          std::error_code& ec) const -> std::uintmax_t {
    ec.clear();
    return std::filesystem::file_size(path, ec);
}

auto FsAdaptor::file_size(std::filesystem::path const& path) const -> std::uintmax_t {
    return std::filesystem::file_size(path);
}

auto FsAdaptor::exists(const std::filesystem::path& path, std::error_code& ec) const -> bool {
    const auto status = query_status(path, ec);
    return !ec && std::filesystem::exists(status);
}

auto FsAdaptor::exists(const std::filesystem::path& path) const -> bool {
    std::error_code ec;
    const bool result = exists(path, ec);
    throw_if_error(ec, path, "exists");
    return result;
}

auto FsAdaptor::is_file(const std::filesystem::path& path, std::error_code& ec) const -> bool {
    const auto status = query_status(path, ec);
    return !ec && std::filesystem::is_regular_file(status);
}

auto FsAdaptor::is_file(const std::filesystem::path& path) const -> bool {
    std::error_code ec;
    const bool result = is_file(path, ec);
    throw_if_error(ec, path, "is_file");
    return result;
}

auto FsAdaptor::is_folder(const std::filesystem::path& path, std::error_code& ec) const -> bool {
    const auto status = query_status(path, ec);
    return !ec && std::filesystem::is_directory(status);
}

auto FsAdaptor::is_folder(const std::filesystem::path& path) const -> bool {
    std::error_code ec;
    const bool result = is_folder(path, ec);
    throw_if_error(ec, path, "is_folder");
    return result;
}

auto FsAdaptor::is_symlink(const std::filesystem::path& path, std::error_code& ec) const -> bool {
    const auto status = query_status(path, ec, false);
    return !ec && std::filesystem::is_symlink(status);
}

auto FsAdaptor::is_symlink(const std::filesystem::path& path) const -> bool {
    std::error_code ec;
    const bool result = is_symlink(path, ec);
    throw_if_error(ec, path, "is_symlink");
    return result;
}

auto FsAdaptor::is_empty(const std::filesystem::path& path, std::error_code& ec) const -> bool {
    ec.clear();
    return std::filesystem::is_empty(path, ec);
}

auto FsAdaptor::is_empty(const std::filesystem::path& path) const -> bool {
    std::error_code ec;
    const bool result = is_empty(path, ec);
    throw_if_error(ec, path, "is_empty");
    return result;
}

auto FsAdaptor::mkdir(const std::filesystem::path& path, std::error_code& ec,
                      bool parents) const -> bool {
    ec.clear();
    return parents ? std::filesystem::create_directories(path, ec)
                   : std::filesystem::create_directory(path, ec);
}

auto FsAdaptor::mkdir(const std::filesystem::path& path, bool parents) const -> bool {
    std::error_code ec;
    const bool created = mkdir(path, ec, parents);
    throw_if_error(ec, path, "mkdir");
    return created;
}

auto FsAdaptor::rm(const std::filesystem::path& path, std::error_code& ec,
                   bool recursive) const -> std::uintmax_t {
    ec.clear();
    if (recursive) {
        const auto count = std::filesystem::remove_all(path, ec);
        return ec ? 0 : count;
    }
    return std::filesystem::remove(path, ec) ? 1u : 0u;
}

auto FsAdaptor::rm(const std::filesystem::path& path, bool recursive) const -> std::uintmax_t {
    std::error_code ec;
    const auto count = rm(path, ec, recursive);
    throw_if_error(ec, path, "rm");
    return count;
}

auto FsAdaptor::listdir(const std::filesystem::path& path, std::error_code& ec,
                        bool recursive) const -> std::vector<std::filesystem::path> {
    ec.clear();
    if (recursive)
        return collect_paths(std::filesystem::recursive_directory_iterator(path, ec), ec);
    return collect_paths(std::filesystem::directory_iterator(path, ec), ec);
}

auto FsAdaptor::listdir(const std::filesystem::path& path,
                        bool recursive) const -> std::vector<std::filesystem::path> {
    std::error_code ec;
    auto paths = listdir(path, ec, recursive);
    throw_if_error(ec, path, "listdir");
    return paths;
}

auto FsAdaptor::append_text(const std::filesystem::path& path, std::string_view data,
                            std::error_code& ec) const -> void {
    ec.clear();
    if (!std::in_range<std::streamsize>(data.size())) {
        ec = std::make_error_code(std::errc::file_too_large);
        return;
    }
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) {
        ec = std::make_error_code(std::errc::io_error);
        return;
    }
    if (!data.empty())
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.close();
    if (!file)
        ec = std::make_error_code(std::errc::io_error);
}

auto FsAdaptor::append_text(const std::filesystem::path& path,
                            std::string_view data) const -> void {
    std::error_code ec;
    append_text(path, data, ec);
    throw_if_error(ec, path, "append_text");
}

auto FsAdaptor::copy_file(const std::filesystem::path& source,
                          const std::filesystem::path& destination, std::error_code& ec,
                          bool overwrite) const -> bool {
    ec.clear();
    return std::filesystem::copy_file(source, destination,
                                      overwrite ? std::filesystem::copy_options::overwrite_existing
                                                : std::filesystem::copy_options::none,
                                      ec);
}

auto FsAdaptor::copy_file(const std::filesystem::path& source,
                          const std::filesystem::path& destination, bool overwrite) const -> bool {
    std::error_code ec;
    const bool copied = copy_file(source, destination, ec, overwrite);
    if (ec)
        throw std::filesystem::filesystem_error("copy_file", source, destination, ec);
    return copied;
}

auto FsAdaptor::rename(const std::filesystem::path& source,
                       const std::filesystem::path& destination,
                       std::error_code& ec) const -> void {
    ec.clear();
    std::filesystem::rename(source, destination, ec);
}

auto FsAdaptor::rename(const std::filesystem::path& source,
                       const std::filesystem::path& destination) const -> void {
    std::error_code ec;
    rename(source, destination, ec);
    if (ec)
        throw std::filesystem::filesystem_error("rename", source, destination, ec);
}

auto FsAdaptor::absolute(const std::filesystem::path& path,
                         std::error_code& ec) const -> std::filesystem::path {
    ec.clear();
    return std::filesystem::absolute(path, ec);
}

auto FsAdaptor::absolute(const std::filesystem::path& path) const -> std::filesystem::path {
    std::error_code ec;
    auto result = absolute(path, ec);
    throw_if_error(ec, path, "absolute");
    return result;
}

auto FsAdaptor::canonical(const std::filesystem::path& path,
                          std::error_code& ec) const -> std::filesystem::path {
    ec.clear();
    return std::filesystem::canonical(path, ec);
}

auto FsAdaptor::canonical(const std::filesystem::path& path) const -> std::filesystem::path {
    std::error_code ec;
    auto result = canonical(path, ec);
    throw_if_error(ec, path, "canonical");
    return result;
}

}  // namespace kitzoo::os
