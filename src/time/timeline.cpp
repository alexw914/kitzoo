// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/time/timeline.cpp
// Description: Implements injectable timelines and cancellable waits with real-time timeouts.
// -----------------------------------------------------------------------------

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
  auto const started = std::chrono::steady_clock::now();
  std::mutex mutex;
  std::condition_variable_any wake;
  std::unique_lock lock(mutex);
  while (!stop.stop_requested()) {
    if (!is_valid())
      return false;
    if (timestamp() >= target)
      return true;
    auto delay = std::chrono::duration_cast<TimeDuration>(std::chrono::milliseconds{1});
    if (timeout != TimeDuration::zero()) {
      auto const elapsed = std::chrono::steady_clock::now() - started;
      if (elapsed >= timeout)
        return false;
      delay = std::min(delay, timeout - std::chrono::duration_cast<TimeDuration>(elapsed));
    }
    wake.wait_for(lock, stop, delay, []() -> bool { return false; });
  }
  return false;
}

auto Timeline::sleep_for(TimeDuration duration, TimeDuration timeout, std::stop_token stop) const -> bool {
  if (timeout < TimeDuration::zero())
    throw std::invalid_argument("Timeline timeout must not be negative");
  if (stop.stop_requested() || !is_valid())
    return false;
  if (duration <= TimeDuration::zero())
    return true;
  auto const start = timestamp().count();
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
  std::lock_guard lock(mutex_);
  timestamp_ = timestamp;
  valid_ = true;
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

Time::Time() : timeline_(std::make_shared<SystemTimeline>()) {}

OffsetTimeline::OffsetTimeline(std::shared_ptr<Timeline> source, TimeDuration offset)
    : source_(std::move(source)), offset_(offset.count()) {
  if (!source_)
    throw std::invalid_argument("OffsetTimeline requires a source");
}

auto OffsetTimeline::set_offset(TimeDuration offset) noexcept -> void {
  offset_.store(offset.count(), std::memory_order_relaxed);
}

auto OffsetTimeline::timestamp(std::string_view key) const -> TimeDuration {
  auto const current = source_->timestamp(key).count();
  auto const offset = offset_.load(std::memory_order_relaxed);
  if ((offset > 0 && current > std::numeric_limits<TimeDuration::rep>::max() - offset) ||
      (offset < 0 && current < std::numeric_limits<TimeDuration::rep>::min() - offset))
    throw std::overflow_error("Timeline clock correction overflow");
  return TimeDuration{current + offset};
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
  if (initialized_)
    return false;
  timeline_ = std::move(timeline);
  initialized_ = true;
  return true;
}

auto Time::selected_timeline() -> std::shared_ptr<Timeline> {
  std::lock_guard lock(mutex_);
  initialized_ = true;
  return timeline_;
}

auto Time::timestamp(std::string_view key) -> TimeDuration {
  return selected_timeline()->timestamp(key);
}

auto Time::is_valid() -> bool {
  return selected_timeline()->is_valid();
}

auto Time::timeline_type() -> TimelineType {
  return selected_timeline()->type();
}

auto Time::sleep_for(TimeDuration duration, TimeDuration timeout, std::stop_token stop) -> bool {
  return selected_timeline()->sleep_for(duration, timeout, stop);
}

} // namespace kitzoo::time
