// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/time/time_watcher.cpp
// Description: Implements named timing and exception-safe scope callbacks on the steady clock.
// -----------------------------------------------------------------------------

#include <kitzoo/time/time_watcher.hpp>

#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace kitzoo::time {

struct TimeWatcher::State {
    struct Record {
        std::optional<std::chrono::steady_clock::time_point> start;
        std::optional<TimeDuration> last;
    };

    mutable std::mutex mutex;
    std::map<std::string, Record, std::less<>> records;
};

TimeWatcher::TimeWatcher() : state_(std::make_shared<State>()) {}

auto TimeWatcher::begin(std::string name) -> void {
    std::lock_guard lock(state_->mutex);
    auto& record = state_->records[std::move(name)];
    if (record.start)
        throw std::logic_error("Named measurement is already active");
    record.start = std::chrono::steady_clock::now();
}

auto TimeWatcher::end(std::string_view name) -> TimeDuration {
    std::lock_guard lock(state_->mutex);
    auto const record = state_->records.find(name);
    if (record == state_->records.end() || !record->second.start)
        throw std::out_of_range("Named measurement is not active");
    auto const elapsed = std::chrono::duration_cast<TimeDuration>(std::chrono::steady_clock::now() -
                                                                  *record->second.start);
    record->second.start.reset();
    record->second.last = elapsed;
    return elapsed;
}

auto TimeWatcher::last_result(std::string_view name) const -> std::optional<TimeDuration> {
    std::lock_guard lock(state_->mutex);
    auto const record = state_->records.find(name);
    return record == state_->records.end() ? std::nullopt : record->second.last;
}

auto TimeWatcher::scope(std::string name, WatchCallback callback) -> Scope {
    {
        std::lock_guard lock(state_->mutex);
        // Reserve the result entry now; destruction never allocates a map node.
        state_->records.try_emplace(name);
    }
    return Scope{state_, std::move(name), std::move(callback)};
}

TimeWatcher::Scope::Scope(std::shared_ptr<State> state, std::string name, WatchCallback callback)
    : state_(std::move(state)),
      name_(std::move(name)),
      callback_(std::move(callback)),
      start_(std::chrono::steady_clock::now()) {}

TimeWatcher::Scope::~Scope() noexcept {
    try {
        finish();
    } catch (...) {
    }
}

TimeWatcher::Scope::Scope(Scope&& other) noexcept
    : state_(std::move(other.state_)),
      name_(std::move(other.name_)),
      callback_(std::move(other.callback_)),
      start_(other.start_),
      result_(other.result_),
      active_(std::exchange(other.active_, false)) {}

auto TimeWatcher::Scope::operator=(Scope&& other) noexcept -> Scope& {
    if (this != &other) {
        try {
            finish();
        } catch (...) {
        }
        state_ = std::move(other.state_);
        name_ = std::move(other.name_);
        callback_ = std::move(other.callback_);
        start_ = other.start_;
        result_ = other.result_;
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}

auto TimeWatcher::Scope::finish() -> TimeDuration {
    if (!active_)
        return result_;
    result_ = std::chrono::duration_cast<TimeDuration>(std::chrono::steady_clock::now() - start_);
    active_ = false;
    {
        std::lock_guard lock(state_->mutex);
        state_->records.find(name_)->second.last = result_;
    }
    if (callback_)
        callback_(name_, result_);
    return result_;
}

}  // namespace kitzoo::time
