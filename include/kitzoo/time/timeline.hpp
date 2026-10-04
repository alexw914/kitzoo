// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/timeline.hpp
// Description: Declares injectable system, feeder and callback timelines with bounded waiting.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIMELINE_HPP
#define KITZOO_TIME_TIMELINE_HPP

#include <kitzoo/time/time.hpp>
#include <kitzoo/utilities/singleton.hpp>

#include <atomic>
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

    virtual auto timestamp(std::string_view key = {}) const -> TimeDuration = 0;

    virtual auto is_valid() const -> bool = 0;

    virtual auto type() const noexcept -> TimelineType { return TimelineType::Custom; }

    // Uses timeline progress, with a steady-clock timeout; zero timeout means unlimited.
    // Invalid timelines return false. Backward jumps postpone the target, not wrap time.
    auto sleep_until(TimeDuration target, TimeDuration timeout = {},
                     std::stop_token stop = {}) const -> bool;

    auto sleep_for(TimeDuration duration, TimeDuration timeout = {},
                   std::stop_token stop = {}) const -> bool;
};

class SystemTimeline final : public Timeline {
public:
    auto timestamp(std::string_view key = {}) const -> TimeDuration override;

    auto is_valid() const -> bool override;

    auto type() const noexcept -> TimelineType override;
};

class FeederTimeline final : public Timeline {
public:
    // Zero is a valid fed timestamp; validity means a sample was supplied.
    auto feed(TimeDuration timestamp) -> void;

    auto timestamp(std::string_view key = {}) const -> TimeDuration override;

    auto is_valid() const -> bool override;

    auto type() const noexcept -> TimelineType override;

private:
    mutable std::mutex mutex_;
    TimeDuration timestamp_{};
    bool valid_ = false;
};

using ClockCallback = std::function<TimeDuration(std::string_view)>;

class CallbackTimeline final : public Timeline {
public:
    explicit CallbackTimeline(ClockCallback callback);

    auto timestamp(std::string_view key = {}) const -> TimeDuration override;

    auto is_valid() const -> bool override;

    auto type() const noexcept -> TimelineType override;

private:
    ClockCallback callback_;
};

// Applies a signed clock correction without modifying the source timeline.
class OffsetTimeline final : public Timeline {
public:
    explicit OffsetTimeline(std::shared_ptr<Timeline> source, TimeDuration offset = {});

    auto set_offset(TimeDuration offset) noexcept -> void;

    auto timestamp(std::string_view key = {}) const -> TimeDuration override;

    auto is_valid() const -> bool override;

    auto type() const noexcept -> TimelineType override;

private:
    std::shared_ptr<Timeline> source_;
    std::atomic<TimeDuration::rep> offset_;
};

// Optional global timeline. Initialize once before its first query; defaults to system.
// Independent timeline objects remain available for tests or multiple replay streams.
class Time : public util::Singleton<Time> {
public:
    auto init(std::shared_ptr<Timeline> timeline) -> bool;

    auto timestamp(std::string_view key = {}) -> TimeDuration;

    auto is_valid() -> bool;

    auto timeline_type() -> TimelineType;

    auto sleep_for(TimeDuration duration, TimeDuration timeout = {},
                   std::stop_token stop = {}) -> bool;

private:
    friend class util::Singleton<Time>;
    Time();

    auto selected_timeline() -> std::shared_ptr<Timeline>;

    std::mutex mutex_;
    bool initialized_ = false;
    std::shared_ptr<Timeline> timeline_;
};

}  // namespace kitzoo::time

#endif  // KITZOO_TIME_TIMELINE_HPP
