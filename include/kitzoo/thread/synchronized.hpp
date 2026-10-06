// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/synchronized.hpp
// Description: Declares Synchronized<T, Mutex>, which guards a value and
//              provides scoped access under its mutex.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_SYNCHRONIZED_HPP
#define KITZOO_THREAD_SYNCHRONIZED_HPP

#include <kitzoo/core/macro.hpp>

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

namespace kitzoo::thread {

namespace detail {

template <typename M>
concept SharedLockable = requires(M& m) {
  m.lock_shared();
  m.unlock_shared();
};

} // namespace detail

template <typename T, typename Mutex = std::mutex>
class Synchronized {
public:
  template <typename... Args>
  explicit Synchronized(Args&&... args) : value_(std::forward<Args>(args)...) {}

  Synchronized(const Synchronized&) = delete;
  auto operator=(const Synchronized&) -> Synchronized& = delete;

  template <typename F>
  auto with_lock(F&& f) -> decltype(auto) {
    std::lock_guard lock{mutex_};
    return std::forward<F>(f)(value_);
  }

  template <typename F>
  auto read(F&& f) const -> decltype(auto)
    requires detail::SharedLockable<Mutex>
  {
    std::shared_lock lock{mutex_};
    return std::forward<F>(f)(value_);
  }

  // Takes a shared lock when Mutex supports one.
  KZ_NODISCARD auto copy() const -> T
    requires std::is_copy_constructible_v<T>
  {
    if constexpr (detail::SharedLockable<Mutex>) {
      std::shared_lock lock{mutex_};
      return value_;
    } else {
      std::lock_guard lock{mutex_};
      return value_;
    }
  }

private:
  mutable Mutex mutex_;
  T value_;
};

} // namespace kitzoo::thread

#endif // KITZOO_THREAD_SYNCHRONIZED_HPP
