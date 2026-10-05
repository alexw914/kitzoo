// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/time.hpp
// Description: Defines time types and declares clocks, calendars and steady timing utilities.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIME_HPP
#define KITZOO_TIME_TIME_HPP

#include <kitzoo/core/macro.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace kitzoo::time {

using TimeStamp = std::chrono::sys_time<std::chrono::nanoseconds>;
using TimeDuration = std::chrono::nanoseconds;

class Stopwatch {
public:
  Stopwatch() noexcept;

  auto reset() noexcept -> void;

  KZ_NODISCARD auto elapsed() const noexcept -> std::chrono::steady_clock::duration;

  template <typename Duration = std::chrono::milliseconds>
  KZ_NODISCARD auto elapsed_as() const noexcept -> Duration {
    return std::chrono::duration_cast<Duration>(elapsed());
  }

private:
  std::chrono::steady_clock::time_point start_;
};

// Targets and remaining durations use the steady clock; remaining may be negative.
class Deadline {
public:
  KZ_NODISCARD static auto after(std::chrono::nanoseconds duration) noexcept -> Deadline;

  KZ_NODISCARD static auto at(std::chrono::steady_clock::time_point target) noexcept -> Deadline;

  KZ_NODISCARD auto expired() const noexcept -> bool;

  KZ_NODISCARD auto remaining() const noexcept -> std::chrono::steady_clock::duration;

  KZ_NODISCARD auto time_point() const noexcept -> std::chrono::steady_clock::time_point;

private:
  explicit Deadline(std::chrono::steady_clock::time_point target) noexcept;

  std::chrono::steady_clock::time_point at_;
};

auto utc_timestamp() -> TimeStamp;

// Epoch is implementation-defined; use differences, never calendar conversion.
auto steady_timestamp() noexcept -> TimeDuration;

// Linux dynamic PTP clock; unsupported platforms/device failures return nullopt.
// Its timescale is hardware-configured and is not automatically converted to UTC.
auto ptp_timestamp(std::string_view device = "/dev/ptp0") -> std::optional<TimeDuration>;

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

auto to_date_time(TimeStamp timestamp, TimeZone zone = TimeZone::Utc) -> DateTime;

// Validates dates and nanosecond range; weekday/day_of_year are derived, not inputs.
// Local dates use the OS timezone and reject dates normalized by mktime (e.g. DST gaps).
auto from_date_time(const DateTime& date, TimeZone zone = TimeZone::Utc) -> TimeStamp;

auto format_timestamp(TimeStamp tp = utc_timestamp(), TimeZone zone = TimeZone::Local,
                      TimestampPrecision precision = TimestampPrecision::Milliseconds) -> std::string;

} // namespace kitzoo::time

#endif // KITZOO_TIME_TIME_HPP
