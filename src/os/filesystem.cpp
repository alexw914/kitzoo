// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/os/filesystem.cpp
// Description: Implements filesystem reading, writing, atomic replacement,
//              temporary directories, and path enumeration.
// -----------------------------------------------------------------------------

#include <kitzoo/os/filesystem.hpp>

#include <cstdio>
#include <fstream>
#include <random>

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
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file) {
        ec = std::make_error_code(std::errc::io_error);
    }
}

auto random_suffix() -> std::string {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;
    return std::to_string(dist(rng));
}

}  // namespace

auto read_file(std::filesystem::path const& path, std::error_code& ec) -> std::string {
    return read_file_impl(path, ec);
}

auto read_file(std::filesystem::path const& path) -> std::string {
    std::error_code ec;
    auto result = read_file_impl(path, ec);
    throw_if_error(ec, path, "read_file");
    return result;
}

auto read_text(std::filesystem::path const& path, std::error_code& ec) -> std::string {
    return read_file_impl(path, ec);
}

auto read_text(std::filesystem::path const& path) -> std::string {
    std::error_code ec;
    auto result = read_file_impl(path, ec);
    throw_if_error(ec, path, "read_text");
    return result;
}

auto write_file(std::filesystem::path const& path, std::span<char const> data,
                std::error_code& ec) -> void {
    write_file_impl(path, data, ec);
}

auto write_file(std::filesystem::path const& path, std::span<char const> data) -> void {
    std::error_code ec;
    write_file_impl(path, data, ec);
    throw_if_error(ec, path, "write_file");
}

auto write_text(std::filesystem::path const& path, std::string_view data,
                std::error_code& ec) -> void {
    std::span const bytes{data.data(), data.size()};
    write_file_impl(path, bytes, ec);
}

auto write_text(std::filesystem::path const& path, std::string_view data) -> void {
    std::error_code ec;
    write_text(path, data, ec);
    throw_if_error(ec, path, "write_text");
}

auto atomic_write(std::filesystem::path const& path, std::span<char const> data,
                  std::error_code& ec) -> void {
    ec.clear();

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(parent, ignored);
    }

    auto const tmp = path.string() + ".tmp." + random_suffix();
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

auto atomic_write(std::filesystem::path const& path, std::span<char const> data) -> void {
    std::error_code ec;
    atomic_write(path, data, ec);
    throw_if_error(ec, path, "atomic_write");
}

auto temp_directory(std::error_code& ec) -> std::filesystem::path {
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

auto temp_directory() -> std::filesystem::path {
    std::error_code ec;
    auto result = temp_directory(ec);
    throw_if_error(ec, "", "temp_directory");
    return result;
}

auto current_path(std::error_code& ec) -> std::filesystem::path {
    return std::filesystem::current_path(ec);
}

auto current_path() -> std::filesystem::path {
    return std::filesystem::current_path();
}

auto set_current_path(std::filesystem::path const& path, std::error_code& ec) -> void {
    std::filesystem::current_path(path, ec);
}

auto set_current_path(std::filesystem::path const& path) -> void {
    std::filesystem::current_path(path);
}

auto create_directories(std::filesystem::path const& path, std::error_code& ec) -> bool {
    return std::filesystem::create_directories(path, ec);
}

auto create_directories(std::filesystem::path const& path) -> bool {
    return std::filesystem::create_directories(path);
}

auto remove_all(std::filesystem::path const& path, std::error_code& ec) -> std::uintmax_t {
    return std::filesystem::remove_all(path, ec);
}

auto remove_all(std::filesystem::path const& path) -> std::uintmax_t {
    std::error_code ec;
    auto count = std::filesystem::remove_all(path, ec);
    throw_if_error(ec, path, "remove_all");
    return count;
}

auto list_directory(std::filesystem::path const& dir,
                    std::error_code& ec) -> std::vector<std::filesystem::path> {
    ec.clear();
    std::filesystem::directory_iterator it{dir, ec};
    if (ec)
        return {};

    std::vector<std::filesystem::path> out;
    std::filesystem::directory_iterator const end;
    for (; it != end; it.increment(ec)) {
        out.push_back(it->path());
        if (ec)
            return {};
    }
    return out;
}

auto list_directory(std::filesystem::path const& dir) -> std::vector<std::filesystem::path> {
    std::error_code ec;
    auto out = list_directory(dir, ec);
    throw_if_error(ec, dir, "list_directory");
    return out;
}

auto file_size(std::filesystem::path const& path, std::error_code& ec) -> std::uintmax_t {
    ec.clear();
    return std::filesystem::file_size(path, ec);
}

auto file_size(std::filesystem::path const& path) -> std::uintmax_t {
    return std::filesystem::file_size(path);
}

}  // namespace kitzoo::os
