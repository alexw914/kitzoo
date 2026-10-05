// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/time_watcher.hpp
// Description: Declares named steady-clock measurements and scoped completion callbacks.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIME_WATCHER_HPP
#define KITZOO_TIME_TIME_WATCHER_HPP

#include <kitzoo/time/time.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace kitzoo::time {

using WatchCallback = std::function<void(std::string_view, TimeDuration)>;

// Names cache the last completed measurement. Operations are synchronized;
// callbacks run outside locks and may query or start another measurement.
class TimeWatcher {
  struct State;

public:
  class Scope {
  public:
    ~Scope() noexcept;

    Scope(Scope const&) = delete;
    auto operator=(Scope const&) -> Scope& = delete;

    Scope(Scope&& other) noexcept;
    auto operator=(Scope&& other) noexcept -> Scope&;

    // Idempotent; explicit completion propagates callback exceptions.
    // Destructor completion suppresses exceptions while retaining the result.
    auto finish() -> TimeDuration;

  private:
    friend class TimeWatcher;
    Scope(std::shared_ptr<State> state, std::string name, WatchCallback callback);

    std::shared_ptr<State> state_;
    std::string name_;
    WatchCallback callback_;
    std::chrono::steady_clock::time_point start_;
    TimeDuration result_{};
    bool active_ = true;
  };

  TimeWatcher();

  // Duplicate active begins throw logic_error; missing ends throw out_of_range.
  auto begin(std::string name) -> void;

  auto end(std::string_view name) -> TimeDuration;

  auto last_result(std::string_view name) const -> std::optional<TimeDuration>;

  // A scope owns shared state and can safely outlive its watcher object.
  auto scope(std::string name, WatchCallback callback = {}) -> Scope;

private:
  std::shared_ptr<State> state_;
};

} // namespace kitzoo::time

#endif // KITZOO_TIME_TIME_WATCHER_HPP
