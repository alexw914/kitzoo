// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/time_watcher.hpp
// Description: Declares named steady-clock measurements and scoped completion callbacks.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIME_WATCHER_HPP
#define KITZOO_TIME_TIME_WATCHER_HPP

#include <kitzoo/memory/memory.hpp>
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

    Scope(const Scope&) = delete;
    auto operator=(const Scope&) -> Scope& = delete;

    Scope(Scope&& other) noexcept;
    auto operator=(Scope&& other) noexcept -> Scope&;

    // Idempotent; explicit completion propagates callback exceptions. Completion
    // from the destructor or move assignment terminates if the callback throws.
    auto finish() -> TimeDuration;

  private:
    friend class TimeWatcher;
    Scope(memory::SharedPtr<State> state, memory::String name, WatchCallback callback);

    memory::SharedPtr<State> state_;
    memory::String name_;
    WatchCallback callback_;
    Stopwatch watch_;
    TimeDuration result_{};
    bool active_ = true;
  };

  TimeWatcher();

  // Duplicate active begins throw logic_error; missing ends throw out_of_range.
  auto begin(std::string_view name) -> void;

  auto end(std::string_view name) -> TimeDuration;

  auto last_result(std::string_view name) const -> std::optional<TimeDuration>;

  // A scope owns shared state and can safely outlive its watcher object.
  auto scope(std::string_view name, WatchCallback callback = {}) -> Scope;

private:
  memory::SharedPtr<State> state_;
};

} // namespace kitzoo::time

#endif // KITZOO_TIME_TIME_WATCHER_HPP
