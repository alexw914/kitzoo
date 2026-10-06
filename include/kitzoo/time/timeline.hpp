// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/timeline.hpp
// Description: Declares injectable system, feeder and callback timelines with bounded waiting.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIMELINE_HPP
#define KITZOO_TIME_TIMELINE_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/core/singleton.hpp>
#include <kitzoo/memory/memory.hpp>
#include <kitzoo/time/time.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string_view>

namespace kitzoo::time {

enum class TimelineType {
  System,
  Feeder,
  Callback,
  Offset,
  Custom,
};

// Nanoseconds in the timeline's domain. Only the system timeline uses Unix UTC.
// Implementations must support concurrent calls; callbacks execute without service locks.
class Timeline {
public:
  virtual ~Timeline() = default;

  KZ_NODISCARD virtual auto timestamp(std::string_view key = {}) const -> TimeDuration = 0;

  KZ_NODISCARD virtual auto is_valid() const -> bool = 0;

  KZ_NODISCARD virtual auto type() const noexcept -> TimelineType { return TimelineType::Custom; }

  // Uses timeline progress, with a steady-clock timeout; zero timeout means unlimited.
  // Invalid timelines return false. Backward jumps postpone the target, not wrap time.
  auto sleep_until(TimeDuration target, TimeDuration timeout = {}, std::stop_token stop = {}) const -> bool;

  auto sleep_for(TimeDuration duration, TimeDuration timeout = {}, std::stop_token stop = {}) const -> bool;

  // Called by sleep_until while timestamp() is below target: blocks until the
  // timeline may have reached target, for at most limit, or until stop. The
  // default sleeps for the remaining gap when type() is System, otherwise polls.
  virtual auto wait_for_progress(TimeDuration now, TimeDuration target, TimeDuration limit, std::stop_token stop) const
      -> void;
};

class SystemTimeline final : public Timeline {
public:
  auto timestamp(std::string_view key = {}) const -> TimeDuration override;

  KZ_NODISCARD auto is_valid() const -> bool override;

  KZ_NODISCARD auto type() const noexcept -> TimelineType override;
};

class FeederTimeline final : public Timeline {
public:
  // Zero is a valid fed timestamp; validity means a sample was supplied.
  auto feed(TimeDuration timestamp) -> void;

  auto timestamp(std::string_view key = {}) const -> TimeDuration override;

  KZ_NODISCARD auto is_valid() const -> bool override;

  KZ_NODISCARD auto type() const noexcept -> TimelineType override;

  // Wakes when a fed timestamp reaches target instead of polling.
  auto wait_for_progress(TimeDuration now, TimeDuration target, TimeDuration limit, std::stop_token stop) const
      -> void override;

private:
  mutable std::mutex mutex_;
  mutable std::condition_variable_any fed_;
  TimeDuration timestamp_{};
  bool valid_ = false;
};

using ClockCallback = std::function<TimeDuration(std::string_view)>;

class CallbackTimeline final : public Timeline {
public:
  explicit CallbackTimeline(ClockCallback callback);

  auto timestamp(std::string_view key = {}) const -> TimeDuration override;

  KZ_NODISCARD auto is_valid() const -> bool override;

  KZ_NODISCARD auto type() const noexcept -> TimelineType override;

private:
  ClockCallback callback_;
};

// Applies a signed clock correction without modifying the source timeline.
class OffsetTimeline final : public Timeline {
public:
  explicit OffsetTimeline(std::shared_ptr<Timeline> source, TimeDuration offset = {});

  auto set_offset(TimeDuration offset) noexcept -> void;

  auto timestamp(std::string_view key = {}) const -> TimeDuration override;

  KZ_NODISCARD auto is_valid() const -> bool override;

  KZ_NODISCARD auto type() const noexcept -> TimelineType override;

  // Waits on the source in its own domain.
  auto wait_for_progress(TimeDuration now, TimeDuration target, TimeDuration limit, std::stop_token stop) const
      -> void override;

private:
  memory::SharedPtr<Timeline> source_;
  std::atomic<TimeDuration::rep> offset_;
};

// Optional global timeline. Initialize once before its first query; defaults to system.
// Independent timeline objects remain available for tests or multiple replay streams.
class Time : public core::Singleton<Time> {
public:
  auto init(std::shared_ptr<Timeline> timeline) -> bool;

  auto timestamp(std::string_view key = {}) -> TimeDuration;

  auto is_valid() -> bool;

  auto timeline_type() -> TimelineType;

  auto sleep_for(TimeDuration duration, TimeDuration timeout = {}, std::stop_token stop = {}) -> bool;

private:
  friend class core::Singleton<Time>;
  Time();

  auto selected_timeline() -> Timeline&;

  std::mutex mutex_;
  memory::SharedPtr<Timeline> timeline_;
  // Set once on selection; timeline_ never changes afterwards, so queries read
  // it without the mutex.
  std::atomic<Timeline*> selected_{nullptr};
};

} // namespace kitzoo::time

#endif // KITZOO_TIME_TIMELINE_HPP
