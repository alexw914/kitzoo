// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/log/file_sink.cpp
// Description: Implements size-based file rollover with timestamped names and
//              periodic age-based cleanup.
// -----------------------------------------------------------------------------

#include <kitzoo/log/logger.hpp>
#include <kitzoo/log/sinks.hpp>
#include <kitzoo/time/time.hpp>
#include <kitzoo/time/timer.hpp>

#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace kitzoo::log {
namespace {

auto all_digits(std::string_view text) -> bool {
  return !text.empty() && text.find_first_not_of("0123456789") == std::string_view::npos;
}

// Matches <stem>_<YYYYmmdd>-<HHMMSS>_<n><extension>.
auto owned_filename(std::string_view name, std::string_view stem, std::string_view extension) -> bool {
  if (name.size() <= stem.size() + extension.size() || !name.starts_with(stem) || !name.ends_with(extension))
    return false;
  name = name.substr(stem.size(), name.size() - stem.size() - extension.size());
  return name.size() > 17 && name[0] == '_' && all_digits(name.substr(1, 8)) && name[9] == '-' &&
         all_digits(name.substr(10, 6)) && name[16] == '_' && all_digits(name.substr(17));
}

auto local_stamp() -> std::string {
  const auto now = time::to_date_time(time::utc_timestamp(), time::TimeZone::Local);
  return fmt::format("{:04}{:02}{:02}-{:02}{:02}{:02}", now.year, now.month, now.day, now.hour, now.minute, now.second);
}

} // namespace

struct RollingFileSink::Impl {
  explicit Impl(const FileSinkOptions& settings) : options(settings) {}

  FileSinkOptions options;
  std::filesystem::path directory;
  std::string stem;
  std::string extension;
  std::filesystem::path current;
  std::ofstream output;
  std::size_t size{0};
  std::string cleanup_error;
  // Serializes cleanups without blocking writers, which use the sink mutex.
  std::mutex cleanup_mutex;
  // Declared last so it stops before the file closes.
  std::optional<time::Timer> cleaner;
};

RollingFileSink::RollingFileSink(const FileSinkOptions& options) : impl_(memory::make_unique<Impl>(options)) {
  const auto filename = options.path.filename();
  if (filename.empty() || filename == "." || filename == ".." ||
      options.path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos)
    throw std::invalid_argument("Log file path must contain a filename");
  if (options.max_size_bytes == 0 || options.max_age < std::chrono::seconds::zero() ||
      options.cleanup_interval < std::chrono::milliseconds::zero())
    throw std::invalid_argument("Log size must be positive and retention durations must not be negative");
  impl_->directory = std::filesystem::absolute(options.path).parent_path();
  std::filesystem::create_directories(impl_->directory);
  impl_->directory = std::filesystem::canonical(impl_->directory);
  impl_->stem = options.path.stem().string();
  impl_->extension = options.path.extension().string();
  open_file();
  cleanup();
  if (options.cleanup_interval != std::chrono::milliseconds::zero()) {
    impl_->cleaner.emplace(options.cleanup_interval);
    impl_->cleaner->start([this] { cleanup(); });
  }
}

RollingFileSink::~RollingFileSink() {
  // Destroying impl_ nulls the pointer before ~Impl joins the timer, so a
  // running cleanup would dereference null; stop it while impl_ is reachable.
  impl_->cleaner.reset();
}

auto RollingFileSink::open_file() -> void {
  const auto stamp = local_stamp();
  std::filesystem::path path;
  std::error_code error;
  for (std::size_t index = 0;; ++index) {
    path = impl_->directory / fmt::format("{}_{}_{}{}", impl_->stem, stamp, index, impl_->extension);
    if (!std::filesystem::exists(std::filesystem::symlink_status(path, error)))
      break;
  }
  std::ofstream next;
  next.exceptions(std::ios::badbit | std::ios::failbit);
  next.open(path, std::ios::binary | std::ios::out);
  if (impl_->output.is_open())
    impl_->output.flush();
  impl_->output = std::move(next);
  impl_->current = std::move(path);
  impl_->size = 0;
}

auto RollingFileSink::sink_it_(const spdlog::details::log_msg& message) -> void {
  spdlog::memory_buf_t formatted;
  formatter_->format(message, formatted);
  if (formatted.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
    throw std::length_error("Log record exceeds stream size range");
  // An oversized record stays intact in one file; the next record starts a new file.
  if (impl_->size != 0 &&
      (impl_->size >= impl_->options.max_size_bytes || formatted.size() > impl_->options.max_size_bytes - impl_->size))
    open_file();
  impl_->output.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
  impl_->size += formatted.size();
}

auto RollingFileSink::flush_() -> void {
  impl_->output.flush();
}

auto RollingFileSink::current_file() -> std::filesystem::path {
  std::lock_guard lock(mutex_);
  return impl_->current;
}

// Scans the directory without the sink mutex so logging continues meanwhile.
// Filesystem errors skip the affected file and are reported through cleanup_error().
auto RollingFileSink::cleanup() -> std::size_t {
  std::lock_guard scan(impl_->cleanup_mutex);
  std::filesystem::path current;
  {
    std::lock_guard lock(mutex_);
    current = impl_->current;
  }
  std::error_code first_error;
  auto record = [&first_error](const std::error_code& error) -> bool {
    if (error && !first_error)
      first_error = error;
    return !error;
  };
  std::size_t removed = 0;
  if (impl_->options.max_age != std::chrono::seconds::zero()) {
    // A file rolled over during the scan is newer than the cutoff, so it stays.
    const auto cutoff = std::filesystem::file_time_type::clock::now() - impl_->options.max_age;
    std::error_code error;
    for (std::filesystem::directory_iterator it{impl_->directory, error}, end; record(error) && it != end;
         it.increment(error)) {
      const auto& entry = *it;
      if (entry.path() == current || !owned_filename(entry.path().filename().string(), impl_->stem, impl_->extension) ||
          !std::filesystem::is_regular_file(entry.symlink_status(error)) || !record(error))
        continue;
      const auto modified = entry.last_write_time(error);
      if (!record(error) || modified > cutoff)
        continue;
      std::error_code remove_error;
      if (std::filesystem::remove(entry.path(), remove_error))
        ++removed;
      record(remove_error);
    }
  }
  std::lock_guard lock(mutex_);
  impl_->cleanup_error = first_error ? first_error.message() : std::string{};
  return removed;
}

auto RollingFileSink::cleanup_error() -> std::string {
  std::lock_guard lock(mutex_);
  return impl_->cleanup_error;
}

} // namespace kitzoo::log
