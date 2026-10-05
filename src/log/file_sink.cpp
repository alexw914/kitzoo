// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/log/file_sink.cpp
// Description: Implements size-based file rollover and periodic retention cleanup.
// -----------------------------------------------------------------------------

#include <kitzoo/log/logger.hpp>
#include <kitzoo/log/sinks.hpp>
#include <kitzoo/time/time.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace kitzoo::log {
namespace {
std::atomic<std::uint64_t> next_sink{0};

auto owned_filename(std::string_view name, std::string_view prefix) -> bool {
  if (!name.starts_with(prefix))
    return false;
  name.remove_prefix(prefix.size());
  for (int part = 0; part < 3; ++part) {
    const auto separator = name.find('.');
    const auto digits = name.substr(0, separator);
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string_view::npos)
      return false;
    if (part == 2)
      return separator == std::string_view::npos;
    if (separator == std::string_view::npos)
      return false;
    name.remove_prefix(separator + 1);
  }
  return false;
}
} // namespace

struct ManagedFileSink::Impl {
  explicit Impl(const FileSinkOptions& settings) : options(settings) {}

  FileSinkOptions options;
  std::filesystem::path directory;
  memory::String prefix;
  memory::String session;
  std::uint64_t sequence{0};
  std::filesystem::path current;
  std::ofstream output;
  std::size_t size{0};
  std::string cleanup_error;
  // Stop and join before closing the file or destroying the sink mutex.
  std::jthread cleaner;
};

ManagedFileSink::ManagedFileSink(const FileSinkOptions& options) : impl_(memory::make_unique<Impl>(options)) {
  if (options.path.empty() || options.path.filename().empty() || options.path.filename() == "." ||
      options.path.filename() == ".." ||
      options.path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos)
    throw std::invalid_argument("Log file path must contain a filename");
  if (options.max_size_bytes == 0 || options.max_age < std::chrono::seconds::zero() ||
      options.cleanup_interval < std::chrono::milliseconds::zero())
    throw std::invalid_argument("Log size must be positive and retention durations must not be negative");
  impl_->directory = std::filesystem::absolute(options.path).parent_path();
  std::filesystem::create_directories(impl_->directory);
  impl_->directory = std::filesystem::canonical(impl_->directory);
  const auto name = options.path.filename().string();
  impl_->prefix.assign(name.data(), name.size());
  impl_->prefix += ".kzlog.";
  const auto session =
      fmt::format("{}.{}", static_cast<std::uint64_t>(time::utc_timestamp().time_since_epoch().count()),
                  next_sink.fetch_add(1, std::memory_order_relaxed));
  impl_->session.assign(session.data(), session.size());
  open_file();
  cleanup_files();
  if (options.cleanup_interval != std::chrono::milliseconds::zero())
    impl_->cleaner = std::jthread([this](std::stop_token stop) -> void { cleanup_loop(stop); });
}

ManagedFileSink::~ManagedFileSink() {
  if (impl_->cleaner.joinable()) {
    impl_->cleaner.request_stop();
    impl_->cleaner.join();
  }
}

auto ManagedFileSink::open_file() -> void {
  std::filesystem::path path;
  do {
    if (impl_->sequence == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("Log file sequence exhausted");
    const auto name = fmt::format("{}{}.{}", impl_->prefix, impl_->session, impl_->sequence++);
    path = impl_->directory / name;
  } while (std::filesystem::exists(std::filesystem::symlink_status(path)));
  std::ofstream next;
  next.exceptions(std::ios::badbit | std::ios::failbit);
  next.open(path, std::ios::binary | std::ios::out);
  if (impl_->output.is_open())
    impl_->output.flush();
  impl_->output = std::move(next);
  impl_->current = std::move(path);
  impl_->size = 0;
}

auto ManagedFileSink::sink_it_(const spdlog::details::log_msg& message) -> void {
  spdlog::memory_buf_t formatted;
  formatter_->format(message, formatted);
  if (formatted.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
    throw std::length_error("Log record exceeds stream size range");
  // Preserve oversized records intact in one file; the next record starts a new file.
  bool rotated = false;
  if (impl_->size != 0 && (impl_->size >= impl_->options.max_size_bytes ||
                           formatted.size() > impl_->options.max_size_bytes - impl_->size)) {
    open_file();
    rotated = true;
  }
  impl_->output.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
  impl_->size += formatted.size();
  // Retention failures must not discard the record that triggered rollover.
  if (rotated)
    cleanup_files();
}

auto ManagedFileSink::flush_() -> void {
  impl_->output.flush();
}

auto ManagedFileSink::current_file() -> std::filesystem::path {
  std::lock_guard lock(mutex_);
  return impl_->current;
}

// Filesystem errors skip the affected file and are reported through cleanup_error().
auto ManagedFileSink::cleanup_files() -> std::size_t {
  struct ClosedFile {
    std::filesystem::path path;
    std::filesystem::file_time_type modified;
  };

  std::error_code first_error;
  auto record = [&first_error](const std::error_code& error) -> bool {
    if (error && !first_error)
      first_error = error;
    return !error;
  };
  auto remove = [&record](const std::filesystem::path& path) -> bool {
    std::error_code error;
    const bool removed = std::filesystem::remove(path, error);
    return record(error) && removed;
  };

  memory::Vector<ClosedFile> files;
  const auto now = std::filesystem::file_time_type::clock::now();
  std::size_t removed = 0;
  std::error_code error;
  for (std::filesystem::directory_iterator it{impl_->directory, error}, end; record(error) && it != end;
       it.increment(error)) {
    const auto& entry = *it;
    if (entry.path() == impl_->current || !std::filesystem::is_regular_file(entry.symlink_status(error)) ||
        !record(error))
      continue;
    if (!owned_filename(entry.path().filename().string(), impl_->prefix))
      continue;
    const auto modified = entry.last_write_time(error);
    if (!record(error))
      continue;
    if (impl_->options.max_age != std::chrono::seconds::zero() && modified <= now &&
        std::chrono::duration_cast<std::chrono::seconds>(now - modified) >= impl_->options.max_age) {
      if (remove(entry.path()))
        ++removed;
    } else {
      files.push_back({entry.path(), modified});
    }
  }
  if (impl_->options.max_files != 0 && files.size() > impl_->options.max_files) {
    std::sort(files.begin(), files.end(), [](const ClosedFile& first, const ClosedFile& second) -> bool {
      return first.modified != second.modified ? first.modified < second.modified : first.path < second.path;
    });
    for (std::size_t index = 0; index < files.size() - impl_->options.max_files; ++index)
      if (remove(files[index].path))
        ++removed;
  }
  impl_->cleanup_error = first_error ? first_error.message() : std::string{};
  return removed;
}

auto ManagedFileSink::cleanup() -> std::size_t {
  std::lock_guard lock(mutex_);
  return cleanup_files();
}

auto ManagedFileSink::cleanup_error() -> std::string {
  std::lock_guard lock(mutex_);
  return impl_->cleanup_error;
}

auto ManagedFileSink::cleanup_loop(std::stop_token stop) -> void {
  std::mutex wait_mutex;
  std::condition_variable_any wake;
  std::unique_lock lock(wait_mutex);
  while (!stop.stop_requested()) {
    wake.wait_for(lock, stop, impl_->options.cleanup_interval, []() -> bool { return false; });
    if (stop.stop_requested())
      break;
    cleanup();
  }
}
} // namespace kitzoo::log
