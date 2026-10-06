// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/time/timeline.cpp
// Description: Implements injectable timelines and cancellable waits with real-time timeouts.
// -----------------------------------------------------------------------------

#include <kitzoo/time/time.hpp>
#include <kitzoo/time/timeline.hpp>

#include <algorithm>
#include <condition_variable>
#include <limits>
#include <stdexcept>
#include <utility>

namespace kitzoo::time {

auto Timeline::sleep_until(TimeDuration target, TimeDuration timeout, std::stop_token stop) const -> bool {
  if (timeout < TimeDuration::zero())
    throw std::invalid_argument("Timeline timeout must not be negative");
  Stopwatch watch;
  while (!stop.stop_requested()) {
    if (!is_valid())
      return false;
    const auto now = timestamp();
    if (now >= target)
      return true;
    // Unlimited waits still return periodically to recheck validity.
    TimeDuration limit = std::chrono::hours{1};
    if (timeout != TimeDuration::zero()) {
      const auto elapsed = std::chrono::duration_cast<TimeDuration>(watch.elapsed());
      if (elapsed >= timeout)
        return false;
      limit = timeout - elapsed;
    }
    wait_for_progress(now, target, limit, stop);
  }
  return false;
}

auto Timeline::wait_for_progress(TimeDuration now, TimeDuration target, TimeDuration limit, std::stop_token stop) const
    -> void {
  // The system clock advances in real time, so it sleeps for the remaining gap,
  // capped to notice clock adjustments; other timelines may move at any rate.
  auto delay = type() == TimelineType::System ? std::min<TimeDuration>(target - now, std::chrono::milliseconds{100})
                                              : TimeDuration{std::chrono::milliseconds{1}};
  delay = std::min(delay, limit);
  std::mutex mutex;
  std::condition_variable_any wake;
  std::unique_lock lock(mutex);
  wake.wait_for(lock, stop, delay, []() -> bool { return false; });
}

auto Timeline::sleep_for(TimeDuration duration, TimeDuration timeout, std::stop_token stop) const -> bool {
  if (timeout < TimeDuration::zero())
    throw std::invalid_argument("Timeline timeout must not be negative");
  if (stop.stop_requested() || !is_valid())
    return false;
  if (duration <= TimeDuration::zero())
    return true;
  const auto start = timestamp().count();
  if (start > std::numeric_limits<TimeDuration::rep>::max() - duration.count())
    throw std::overflow_error("Timeline sleep target overflow");
  return sleep_until(TimeDuration{start + duration.count()}, timeout, stop);
}

auto SystemTimeline::timestamp(std::string_view) const -> TimeDuration {
  return utc_timestamp().time_since_epoch();
}

auto SystemTimeline::is_valid() const -> bool {
  return true;
}

auto SystemTimeline::type() const noexcept -> TimelineType {
  return TimelineType::System;
}

auto FeederTimeline::feed(TimeDuration timestamp) -> void {
  {
    std::lock_guard lock(mutex_);
    timestamp_ = timestamp;
    valid_ = true;
  }
  fed_.notify_all();
}

auto FeederTimeline::wait_for_progress(TimeDuration, TimeDuration target, TimeDuration limit,
                                       std::stop_token stop) const -> void {
  std::unique_lock lock(mutex_);
  fed_.wait_for(lock, stop, limit, [this, target]() -> bool { return timestamp_ >= target; });
}

auto FeederTimeline::timestamp(std::string_view) const -> TimeDuration {
  std::lock_guard lock(mutex_);
  return timestamp_;
}

auto FeederTimeline::is_valid() const -> bool {
  std::lock_guard lock(mutex_);
  return valid_;
}

auto FeederTimeline::type() const noexcept -> TimelineType {
  return TimelineType::Feeder;
}

CallbackTimeline::CallbackTimeline(ClockCallback callback) : callback_(std::move(callback)) {
  if (!callback_)
    throw std::invalid_argument("CallbackTimeline requires a callback");
}

auto CallbackTimeline::timestamp(std::string_view key) const -> TimeDuration {
  return callback_(key);
}

auto CallbackTimeline::is_valid() const -> bool {
  return true;
}

auto CallbackTimeline::type() const noexcept -> TimelineType {
  return TimelineType::Callback;
}

Time::Time() : timeline_(memory::make_shared<SystemTimeline>()) {}

OffsetTimeline::OffsetTimeline(std::shared_ptr<Timeline> source, TimeDuration offset)
    : source_(std::move(source)), offset_(offset.count()) {
  if (!source_)
    throw std::invalid_argument("OffsetTimeline requires a source");
}

auto OffsetTimeline::set_offset(TimeDuration offset) noexcept -> void {
  offset_.store(offset.count(), std::memory_order_relaxed);
}

auto OffsetTimeline::timestamp(std::string_view key) const -> TimeDuration {
  const auto current = source_->timestamp(key).count();
  const auto offset = offset_.load(std::memory_order_relaxed);
  if ((offset > 0 && current > std::numeric_limits<TimeDuration::rep>::max() - offset) ||
      (offset < 0 && current < std::numeric_limits<TimeDuration::rep>::min() - offset))
    throw std::overflow_error("Timeline clock correction overflow");
  return TimeDuration{current + offset};
}

auto OffsetTimeline::wait_for_progress(TimeDuration now, TimeDuration target, TimeDuration limit,
                                       std::stop_token stop) const -> void {
  const auto offset = offset_.load(std::memory_order_relaxed);
  // Saturates instead of overflowing; sleep_until rechecks the real timestamp.
  const auto shift = [offset](TimeDuration value) -> TimeDuration {
    constexpr auto max = std::numeric_limits<TimeDuration::rep>::max();
    constexpr auto min = std::numeric_limits<TimeDuration::rep>::min();
    if (offset < 0 && value.count() > max + offset)
      return TimeDuration{max};
    if (offset > 0 && value.count() < min + offset)
      return TimeDuration{min};
    return TimeDuration{value.count() - offset};
  };
  // A bounded wait notices offset changes made while waiting.
  source_->wait_for_progress(shift(now), shift(target), std::min<TimeDuration>(limit, std::chrono::milliseconds{100}),
                             stop);
}

auto OffsetTimeline::is_valid() const -> bool {
  return source_->is_valid();
}

auto OffsetTimeline::type() const noexcept -> TimelineType {
  return TimelineType::Offset;
}

auto Time::init(std::shared_ptr<Timeline> timeline) -> bool {
  if (!timeline)
    return false;
  std::lock_guard lock(mutex_);
  if (selected_.load(std::memory_order_relaxed))
    return false;
  timeline_ = std::move(timeline);
  selected_.store(timeline_.get(), std::memory_order_release);
  return true;
}

auto Time::selected_timeline() -> Timeline& {
  if (auto* selected = selected_.load(std::memory_order_acquire))
    return *selected;
  std::lock_guard lock(mutex_);
  if (!selected_.load(std::memory_order_relaxed))
    selected_.store(timeline_.get(), std::memory_order_release);
  return *timeline_;
}

auto Time::timestamp(std::string_view key) -> TimeDuration {
  return selected_timeline().timestamp(key);
}

auto Time::is_valid() -> bool {
  return selected_timeline().is_valid();
}

auto Time::timeline_type() -> TimelineType {
  return selected_timeline().type();
}

auto Time::sleep_for(TimeDuration duration, TimeDuration timeout, std::stop_token stop) -> bool {
  return selected_timeline().sleep_for(duration, timeout, stop);
}

} // namespace kitzoo::time
