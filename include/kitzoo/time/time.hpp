// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/time.hpp
// Description: Declares stopwatches, deadlines, calendar conversion and nanosecond clocks.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIME_HPP
#define KITZOO_TIME_TIME_HPP

#include <kitzoo/core/macro.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace kitzoo::time {

class Stopwatch {
public:
  Stopwatch() noexcept : start_{std::chrono::steady_clock::now()} {}

  auto reset() noexcept -> void { start_ = std::chrono::steady_clock::now(); }

  KZ_NODISCARD auto elapsed() const noexcept -> std::chrono::steady_clock::duration {
    return std::chrono::steady_clock::now() - start_;
  }

  template <typename Duration = std::chrono::milliseconds>
  KZ_NODISCARD auto elapsed_as() const noexcept -> Duration {
    return std::chrono::duration_cast<Duration>(elapsed());
  }

private:
  std::chrono::steady_clock::time_point start_;
};

class Deadline {
public:
  KZ_NODISCARD static auto after(std::chrono::nanoseconds d) noexcept -> Deadline {
    return Deadline{std::chrono::steady_clock::now() + d};
  }

  KZ_NODISCARD static auto at(std::chrono::steady_clock::time_point tp) noexcept -> Deadline { return Deadline{tp}; }

  KZ_NODISCARD auto expired() const noexcept -> bool { return std::chrono::steady_clock::now() >= at_; }

  KZ_NODISCARD auto remaining() const noexcept -> std::chrono::steady_clock::duration {
    return at_ - std::chrono::steady_clock::now();
  }

  KZ_NODISCARD auto time_point() const noexcept -> std::chrono::steady_clock::time_point { return at_; }

private:
  explicit Deadline(std::chrono::steady_clock::time_point tp) noexcept : at_{tp} {}

  std::chrono::steady_clock::time_point at_;
};

using TimeStamp = std::chrono::sys_time<std::chrono::nanoseconds>;
using TimeDuration = std::chrono::nanoseconds;

enum class TimeZone {
  Utc,
  Local,
};

enum class TimestampPrecision {
  Seconds,
  Milliseconds,
  Microseconds,
  Nanoseconds,
};

struct DateTime {
  int year = 1970;
  unsigned month = 1;
  unsigned day = 1;
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
  unsigned nanosecond = 0;
  unsigned weekday = 4; // Sunday = 0; computed by to_date_time().
  unsigned day_of_year = 1;
};

auto utc_timestamp() -> TimeStamp;

// Epoch is implementation-defined; use differences, never calendar conversion.
auto steady_timestamp() noexcept -> TimeDuration;

auto to_date_time(TimeStamp timestamp, TimeZone zone = TimeZone::Utc) -> DateTime;

// Validates dates and nanosecond range; weekday/day_of_year are derived, not inputs.
// Local dates use the OS timezone and reject dates normalized by mktime (e.g. DST gaps).
auto from_date_time(DateTime const& date, TimeZone zone = TimeZone::Utc) -> TimeStamp;

auto format_timestamp(TimeStamp tp = utc_timestamp(), TimeZone zone = TimeZone::Local,
                      TimestampPrecision precision = TimestampPrecision::Milliseconds) -> std::string;

// Linux dynamic PTP clock; unsupported platforms/device failures return nullopt.
// Its timescale is hardware-configured and is not automatically converted to UTC.
auto ptp_timestamp(std::string_view device = "/dev/ptp0") -> std::optional<TimeDuration>;

} // namespace kitzoo::time

#endif // KITZOO_TIME_TIME_HPP
