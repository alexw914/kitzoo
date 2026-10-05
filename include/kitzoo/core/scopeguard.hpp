// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/scopeguard.hpp
// Description: Declares folly-style scope guards and KZ_SCOPE_EXIT/FAIL/SUCCESS
//              macros that run code on scope exit, failure, or success.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_SCOPEGUARD_HPP
#define KITZOO_CORE_SCOPEGUARD_HPP

#include <kitzoo/core/macro.hpp>

#include <concepts>
#include <exception>
#include <type_traits>
#include <utility>

namespace kitzoo::core {

enum class ScopeRun {
  Always,
  OnFailure,
  OnSuccess,
};

// Modelled on folly::ScopeGuard. The callable runs from the destructor and must
// not throw; prefer the KZ_SCOPE_EXIT/FAIL/SUCCESS macros below.
template <std::invocable F>
class KZ_NODISCARD ScopeGuard {
  // A throwing move could fail after the guarded action and lose the cleanup.
  static_assert(std::is_nothrow_move_constructible_v<F>, "ScopeGuard callables must be nothrow movable");

public:
  explicit ScopeGuard(F callback, ScopeRun when = ScopeRun::Always) noexcept
      : callback_(std::move(callback)), when_(when) {}

  ScopeGuard(const ScopeGuard&) = delete;
  auto operator=(const ScopeGuard&) -> ScopeGuard& = delete;

  ~ScopeGuard() {
    if (!dismissed_ && should_run())
      callback_();
  }

  auto dismiss() noexcept -> void { dismissed_ = true; }

private:
  auto should_run() const noexcept -> bool {
    const bool unwinding = std::uncaught_exceptions() > exceptions_;
    switch (when_) {
    case ScopeRun::Always:
      return true;
    case ScopeRun::OnFailure:
      return unwinding;
    case ScopeRun::OnSuccess:
      return !unwinding;
    }
    return false;
  }

  F callback_;
  ScopeRun when_;
  int exceptions_ = std::uncaught_exceptions();
  bool dismissed_ = false;
};

namespace detail {

enum class ScopeExitTag {
};
enum class ScopeFailTag {
};
enum class ScopeSuccessTag {
};

template <std::invocable F>
auto operator+(ScopeExitTag, F callback) -> ScopeGuard<F> {
  return ScopeGuard<F>{std::move(callback), ScopeRun::Always};
}

template <std::invocable F>
auto operator+(ScopeFailTag, F callback) -> ScopeGuard<F> {
  return ScopeGuard<F>{std::move(callback), ScopeRun::OnFailure};
}

template <std::invocable F>
auto operator+(ScopeSuccessTag, F callback) -> ScopeGuard<F> {
  return ScopeGuard<F>{std::move(callback), ScopeRun::OnSuccess};
}

} // namespace detail

} // namespace kitzoo::core

#define KZ_SCOPE_GUARD_CONCAT_(a, b) a##b
#define KZ_SCOPE_GUARD_CONCAT(a, b) KZ_SCOPE_GUARD_CONCAT_(a, b)
#define KZ_SCOPE_GUARD_VARIABLE KZ_SCOPE_GUARD_CONCAT(kz_scope_guard_, __COUNTER__)

// Usage: KZ_SCOPE_FAIL { rollback(); }; The block captures by reference.
#define KZ_SCOPE_EXIT const auto KZ_SCOPE_GUARD_VARIABLE = ::kitzoo::core::detail::ScopeExitTag{} + [&]() noexcept
#define KZ_SCOPE_FAIL const auto KZ_SCOPE_GUARD_VARIABLE = ::kitzoo::core::detail::ScopeFailTag{} + [&]() noexcept
#define KZ_SCOPE_SUCCESS const auto KZ_SCOPE_GUARD_VARIABLE = ::kitzoo::core::detail::ScopeSuccessTag{} + [&]() noexcept

#endif // KITZOO_CORE_SCOPEGUARD_HPP
