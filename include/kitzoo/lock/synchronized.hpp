// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/lock/synchronized.hpp
// Description: Declares Synchronized<T, Mutex>, which guards a value and
//              provides scoped access under its mutex.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

namespace kitzoo::lock {

template <typename T, typename Mutex = std::mutex>
class Synchronized {
public:
    template <typename... Args>
    explicit Synchronized(Args&&... args) : value_(std::forward<Args>(args)...) {}

    Synchronized(Synchronized const&) = delete;
    auto operator=(Synchronized const&) -> Synchronized& = delete;

    template <typename F>
    auto with_lock(F&& f) -> decltype(auto) {
        std::lock_guard lock{mutex_};
        return std::forward<F>(f)(value_);
    }
    template <typename F>
    auto read(F&& f) const -> decltype(auto)
        requires requires(Mutex const& m) { std::shared_lock{m}; }
    {
        std::shared_lock lock{mutex_};
        return std::forward<F>(f)(value_);
    }
    KZ_NODISCARD auto copy() const -> T
        requires std::is_copy_constructible_v<T>
    {
        std::lock_guard lock{mutex_};
        return value_;
    }

private:
    mutable Mutex mutex_;
    T value_;
};

}  // namespace kitzoo::lock
