// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/time/time.cpp
// Description: Implements basic clocks, calendar conversion and steady timing utilities.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/time/time.hpp>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <utility>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace kitzoo::time {
namespace {

constexpr std::int64_t kNanosecondsPerSecond = 1000000000;

auto checked_timestamp(std::int64_t seconds, unsigned fraction) -> TimeStamp {
  auto const base_seconds = seconds < 0 ? seconds + 1 : seconds;
  auto const offset = static_cast<std::int64_t>(fraction) - (seconds < 0 ? kNanosecondsPerSecond : 0);
  auto const minimum = std::numeric_limits<std::int64_t>::min();
  auto const maximum = std::numeric_limits<std::int64_t>::max();
  if (base_seconds < minimum / kNanosecondsPerSecond || base_seconds > maximum / kNanosecondsPerSecond)
    throw std::out_of_range("Date exceeds nanosecond timestamp range");
  auto const base = base_seconds * kNanosecondsPerSecond;
  if ((offset > 0 && base > maximum - offset) || (offset < 0 && base < minimum - offset))
    throw std::out_of_range("Date exceeds nanosecond timestamp range");
  return TimeStamp{TimeDuration{base + offset}};
}

auto system_timestamp(std::chrono::system_clock::time_point value) -> TimeStamp {
  auto const seconds = std::chrono::floor<std::chrono::seconds>(value);
  auto fraction = std::chrono::duration_cast<TimeDuration>(value.time_since_epoch() % std::chrono::seconds{1}).count();
  if (fraction < 0)
    fraction += kNanosecondsPerSecond;
  return checked_timestamp(seconds.time_since_epoch().count(), static_cast<unsigned>(fraction));
}

auto local_date(std::time_t seconds, unsigned fraction) -> DateTime {
  std::tm date{};
#if defined(_WIN32)
  if (localtime_s(&date, &seconds) != 0)
#else
  if (!localtime_r(&seconds, &date))
#endif
    throw std::out_of_range("OS local-time conversion failed");
  return {date.tm_year + 1900,
          static_cast<unsigned>(date.tm_mon + 1),
          static_cast<unsigned>(date.tm_mday),
          static_cast<unsigned>(date.tm_hour),
          static_cast<unsigned>(date.tm_min),
          static_cast<unsigned>(date.tm_sec),
          fraction,
          static_cast<unsigned>(date.tm_wday),
          static_cast<unsigned>(date.tm_yday + 1)};
}

} // namespace

Stopwatch::Stopwatch() noexcept : start_(std::chrono::steady_clock::now()) {}

auto Stopwatch::reset() noexcept -> void {
  start_ = std::chrono::steady_clock::now();
}

auto Stopwatch::elapsed() const noexcept -> std::chrono::steady_clock::duration {
  return std::chrono::steady_clock::now() - start_;
}

Deadline::Deadline(std::chrono::steady_clock::time_point target) noexcept : at_(target) {}

auto Deadline::after(std::chrono::nanoseconds duration) noexcept -> Deadline {
  return Deadline{std::chrono::steady_clock::now() + duration};
}

auto Deadline::at(std::chrono::steady_clock::time_point target) noexcept -> Deadline {
  return Deadline{target};
}

auto Deadline::expired() const noexcept -> bool {
  return std::chrono::steady_clock::now() >= at_;
}

auto Deadline::remaining() const noexcept -> std::chrono::steady_clock::duration {
  return at_ - std::chrono::steady_clock::now();
}

auto Deadline::time_point() const noexcept -> std::chrono::steady_clock::time_point {
  return at_;
}

auto utc_timestamp() -> TimeStamp {
  return system_timestamp(std::chrono::system_clock::now());
}

auto steady_timestamp() noexcept -> TimeDuration {
  return std::chrono::duration_cast<TimeDuration>(std::chrono::steady_clock::now().time_since_epoch());
}

auto ptp_timestamp(std::string_view device) -> std::optional<TimeDuration> {
#if defined(__linux__)
  if (device.empty() || device.find('\0') != std::string_view::npos)
    return std::nullopt;
  memory::String const path{device};
  auto const descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (descriptor < 0)
    return std::nullopt;
  auto const clock = static_cast<clockid_t>((~static_cast<unsigned>(descriptor) << 3) | 3U);
  timespec value{};
  auto const status = clock_gettime(clock, &value);
  close(descriptor);
  if (status != 0 || value.tv_nsec < 0 || value.tv_nsec >= kNanosecondsPerSecond)
    return std::nullopt;
  try {
    return checked_timestamp(value.tv_sec, static_cast<unsigned>(value.tv_nsec)).time_since_epoch();
  } catch (std::out_of_range const&) {
    return std::nullopt;
  }
#else
  (void)device;
  return std::nullopt;
#endif
}

auto to_date_time(TimeStamp timestamp, TimeZone zone) -> DateTime {
  auto const seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
  auto fraction = timestamp.time_since_epoch().count() % kNanosecondsPerSecond;
  if (fraction < 0)
    fraction += kNanosecondsPerSecond;
  if (zone == TimeZone::Local) {
    auto const count = seconds.time_since_epoch().count();
    if (!std::in_range<std::time_t>(count))
      throw std::out_of_range("Timestamp exceeds OS time_t range");
    return local_date(static_cast<std::time_t>(count), static_cast<unsigned>(fraction));
  }
  if (zone != TimeZone::Utc)
    throw std::invalid_argument("Unknown timezone");
  auto const day = std::chrono::floor<std::chrono::days>(seconds);
  std::chrono::year_month_day const date{day};
  std::chrono::hh_mm_ss const clock{seconds - day};
  auto const start_of_year = std::chrono::sys_days{date.year() / std::chrono::January / 1};
  return {static_cast<int>(date.year()),
          static_cast<unsigned>(date.month()),
          static_cast<unsigned>(date.day()),
          static_cast<unsigned>(clock.hours().count()),
          static_cast<unsigned>(clock.minutes().count()),
          static_cast<unsigned>(clock.seconds().count()),
          static_cast<unsigned>(fraction),
          std::chrono::weekday{day}.c_encoding(),
          static_cast<unsigned>((day - start_of_year).count() + 1)};
}

auto from_date_time(DateTime const& date, TimeZone zone) -> TimeStamp {
  if (date.year < -32767 || date.year > 32767 || date.month < 1 || date.month > 12 || date.day < 1 || date.day > 31 ||
      date.hour > 23 || date.minute > 59 || date.second > 59 ||
      date.nanosecond >= static_cast<unsigned>(kNanosecondsPerSecond))
    throw std::invalid_argument("Invalid date or clock fields");
  std::chrono::year_month_day const calendar{std::chrono::year{date.year}, std::chrono::month{date.month},
                                             std::chrono::day{date.day}};
  if (!calendar.ok())
    throw std::invalid_argument("Invalid calendar date");
  if (zone == TimeZone::Utc) {
    auto const seconds =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::sys_days{calendar}.time_since_epoch()) +
        std::chrono::hours{date.hour} + std::chrono::minutes{date.minute} + std::chrono::seconds{date.second};
    return checked_timestamp(seconds.count(), date.nanosecond);
  }
  if (zone != TimeZone::Local)
    throw std::invalid_argument("Unknown timezone");
  std::tm local{};
  local.tm_year = date.year - 1900;
  local.tm_mon = static_cast<int>(date.month) - 1;
  local.tm_mday = static_cast<int>(date.day);
  local.tm_hour = static_cast<int>(date.hour);
  local.tm_min = static_cast<int>(date.minute);
  local.tm_sec = static_cast<int>(date.second);
  local.tm_isdst = -1;
  auto const seconds = std::mktime(&local);
  auto const converted = local_date(seconds, date.nanosecond);
  if (converted.year != date.year || converted.month != date.month || converted.day != date.day ||
      converted.hour != date.hour || converted.minute != date.minute || converted.second != date.second)
    throw std::invalid_argument("Local date is not representable (possible DST gap)");
  if (!std::in_range<std::int64_t>(seconds))
    throw std::out_of_range("Local date exceeds timestamp range");
  return checked_timestamp(static_cast<std::int64_t>(seconds), date.nanosecond);
}

auto format_timestamp(TimeStamp tp, TimeZone zone, TimestampPrecision precision) -> std::string {
  auto const date = to_date_time(tp, zone);
  char output[64];
  std::snprintf(output, sizeof(output), "%04d-%02u-%02u %02u:%02u:%02u", date.year, date.month, date.day, date.hour,
                date.minute, date.second);
  std::string result{output};
  unsigned digits = 0;
  unsigned divisor = 1;
  switch (precision) {
  case TimestampPrecision::Seconds:
    return result;
  case TimestampPrecision::Milliseconds:
    digits = 3;
    divisor = 1000000;
    break;
  case TimestampPrecision::Microseconds:
    digits = 6;
    divisor = 1000;
    break;
  case TimestampPrecision::Nanoseconds:
    digits = 9;
    break;
  default:
    throw std::invalid_argument("Unknown timestamp precision");
  }
  std::snprintf(output, sizeof(output), ".%0*u", static_cast<int>(digits), date.nanosecond / divisor);
  result += output;
  return result;
}

} // namespace kitzoo::time
