// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/os/filesys.cpp
// Description: Implements filesystem reading, writing, atomic replacement,
//              temporary directories, and path enumeration.
// -----------------------------------------------------------------------------

#include <kitzoo/core/scopeguard.hpp>
#include <kitzoo/os/filesys.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <random>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace kitzoo::os {

namespace {

// File streams open through the C runtime, which reports the failure in errno.
auto open_error() -> std::error_code {
  return errno != 0 ? std::error_code{errno, std::generic_category()} : std::make_error_code(std::errc::io_error);
}

auto open_input(const std::filesystem::path& path, std::error_code& ec) -> std::ifstream {
  errno = 0;
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  if (!file)
    ec = open_error();
  return file;
}

auto open_output(const std::filesystem::path& path, std::error_code& ec) -> std::ofstream {
  errno = 0;
  std::ofstream file{path, std::ios::binary | std::ios::trunc};
  if (!file)
    ec = open_error();
  return file;
}

auto sync_file(const std::filesystem::path& path, std::error_code& ec) -> void {
#if defined(_WIN32)
  const auto handle = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    ec = std::error_code{static_cast<int>(::GetLastError()), std::system_category()};
    return;
  }
  KZ_SCOPE_EXIT {
    ::CloseHandle(handle);
  };
  if (!::FlushFileBuffers(handle))
    ec = std::error_code{static_cast<int>(::GetLastError()), std::system_category()};
#else
  const auto fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    ec = std::error_code{errno, std::generic_category()};
    return;
  }
  KZ_SCOPE_EXIT {
    ::close(fd);
  };
#if defined(__APPLE__)
  // fsync on macOS does not flush the drive cache.
  const auto result = ::fcntl(fd, F_FULLFSYNC) == 0 ? 0 : ::fsync(fd);
#else
  const auto result = ::fsync(fd);
#endif
  if (result != 0)
    ec = std::error_code{errno, std::generic_category()};
#endif
}

auto throw_if_error(const std::error_code& ec, const std::filesystem::path& path, const char* op) -> void {
  if (ec)
    throw std::filesystem::filesystem_error{op, path, ec};
}

auto read_file_impl(const std::filesystem::path& path, std::error_code& ec) -> std::string {
  ec.clear();
  auto file = open_input(path, ec);
  if (ec)
    return {};

  const auto size = file.tellg();
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

auto write_file_impl(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void {
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

auto query_status(const std::filesystem::path& path, std::error_code& ec, bool follow = true)
    -> std::filesystem::file_status {
  ec.clear();
  auto status = follow ? std::filesystem::status(path, ec) : std::filesystem::symlink_status(path, ec);
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

} // namespace

auto read_file(const std::filesystem::path& path, std::error_code& ec) -> std::string {
  return read_file_impl(path, ec);
}

auto read_file(const std::filesystem::path& path) -> std::string {
  std::error_code ec;
  auto result = read_file_impl(path, ec);
  throw_if_error(ec, path, "read_file");
  return result;
}

auto write_file(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void {
  write_file_impl(path, data, ec);
}

auto write_file(const std::filesystem::path& path, std::string_view data) -> void {
  std::error_code ec;
  write_file_impl(path, data, ec);
  throw_if_error(ec, path, "write_file");
}

auto atomic_write(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void {
  ec.clear();

  if (auto parent = path.parent_path(); !parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec)
      return;
  }

  auto tmp = path;
  tmp += ".tmp." + random_suffix();
  write_file_impl(tmp, data, ec);
  if (!ec)
    sync_file(tmp, ec);
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
#if !defined(_WIN32)
  // Persist the rename; failures here do not affect the already replaced file.
  if (!ec) {
    std::error_code dir_ec;
    sync_file(path.parent_path().empty() ? "." : path.parent_path(), dir_ec);
  }
#endif
}

auto atomic_write(const std::filesystem::path& path, std::string_view data) -> void {
  std::error_code ec;
  os::atomic_write(path, data, ec);
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
  auto result = os::temp_directory(ec);
  throw_if_error(ec, "", "temp_directory");
  return result;
}

auto path_exists(const std::filesystem::path& path, std::error_code& ec) -> bool {
  const auto status = query_status(path, ec);
  return !ec && std::filesystem::exists(status);
}

auto path_exists(const std::filesystem::path& path) -> bool {
  std::error_code ec;
  const bool result = os::path_exists(path, ec);
  throw_if_error(ec, path, "path_exists");
  return result;
}

auto is_regular(const std::filesystem::path& path, std::error_code& ec) -> bool {
  const auto status = query_status(path, ec);
  return !ec && std::filesystem::is_regular_file(status);
}

auto is_regular(const std::filesystem::path& path) -> bool {
  std::error_code ec;
  const bool result = os::is_regular(path, ec);
  throw_if_error(ec, path, "is_regular");
  return result;
}

auto is_dir(const std::filesystem::path& path, std::error_code& ec) -> bool {
  const auto status = query_status(path, ec);
  return !ec && std::filesystem::is_directory(status);
}

auto is_dir(const std::filesystem::path& path) -> bool {
  std::error_code ec;
  const bool result = os::is_dir(path, ec);
  throw_if_error(ec, path, "is_dir");
  return result;
}

auto is_link(const std::filesystem::path& path, std::error_code& ec) -> bool {
  const auto status = query_status(path, ec, false);
  return !ec && std::filesystem::is_symlink(status);
}

auto is_link(const std::filesystem::path& path) -> bool {
  std::error_code ec;
  const bool result = os::is_link(path, ec);
  throw_if_error(ec, path, "is_link");
  return result;
}

auto make_directory(const std::filesystem::path& path, std::error_code& ec, bool parents) -> bool {
  ec.clear();
  return parents ? std::filesystem::create_directories(path, ec) : std::filesystem::create_directory(path, ec);
}

auto make_directory(const std::filesystem::path& path, bool parents) -> bool {
  std::error_code ec;
  const bool created = os::make_directory(path, ec, parents);
  throw_if_error(ec, path, "make_directory");
  return created;
}

auto remove_path(const std::filesystem::path& path, std::error_code& ec, bool recursive) -> std::uintmax_t {
  ec.clear();
  if (recursive) {
    const auto count = std::filesystem::remove_all(path, ec);
    return ec ? 0 : count;
  }
  return std::filesystem::remove(path, ec) ? 1u : 0u;
}

auto remove_path(const std::filesystem::path& path, bool recursive) -> std::uintmax_t {
  std::error_code ec;
  const auto count = os::remove_path(path, ec, recursive);
  throw_if_error(ec, path, "remove_path");
  return count;
}

auto list_directory(const std::filesystem::path& path, std::error_code& ec, bool recursive)
    -> std::vector<std::filesystem::path> {
  ec.clear();
  if (recursive)
    return collect_paths(std::filesystem::recursive_directory_iterator(path, ec), ec);
  return collect_paths(std::filesystem::directory_iterator(path, ec), ec);
}

auto list_directory(const std::filesystem::path& path, bool recursive) -> std::vector<std::filesystem::path> {
  std::error_code ec;
  auto paths = os::list_directory(path, ec, recursive);
  throw_if_error(ec, path, "list_directory");
  return paths;
}

auto append_file(const std::filesystem::path& path, std::string_view data, std::error_code& ec) -> void {
  ec.clear();
  if (!std::in_range<std::streamsize>(data.size())) {
    ec = std::make_error_code(std::errc::file_too_large);
    return;
  }
  errno = 0;
  std::ofstream file(path, std::ios::binary | std::ios::app);
  if (!file) {
    ec = open_error();
    return;
  }
  if (!data.empty())
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
  file.close();
  if (!file)
    ec = std::make_error_code(std::errc::io_error);
}

auto append_file(const std::filesystem::path& path, std::string_view data) -> void {
  std::error_code ec;
  os::append_file(path, data, ec);
  throw_if_error(ec, path, "append_file");
}

auto copy_file_to(const std::filesystem::path& source, const std::filesystem::path& destination, std::error_code& ec,
                  bool overwrite) -> bool {
  ec.clear();
  return std::filesystem::copy_file(
      source, destination,
      overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none, ec);
}

auto copy_file_to(const std::filesystem::path& source, const std::filesystem::path& destination, bool overwrite)
    -> bool {
  std::error_code ec;
  const bool copied = os::copy_file_to(source, destination, ec, overwrite);
  if (ec)
    throw std::filesystem::filesystem_error("copy_file_to", source, destination, ec);
  return copied;
}

} // namespace kitzoo::os
