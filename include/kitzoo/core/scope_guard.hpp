// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/scope_guard.hpp
// Description: Declares a guard that runs a cleanup callable when its scope
//              exits unless dismissed.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_SCOPE_GUARD_HPP
#define KITZOO_CORE_SCOPE_GUARD_HPP

#include <type_traits>
#include <utility>

namespace kitzoo::core {

// The cleanup runs from the destructor, so an exception escaping it terminates.
template <typename F>
  requires std::is_invocable_v<F&>
class ScopeGuard {
public:
  explicit ScopeGuard(F cleanup) noexcept(std::is_nothrow_move_constructible_v<F>) : cleanup_(std::move(cleanup)) {}

  ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
      : cleanup_(std::move(other.cleanup_)), active_(std::exchange(other.active_, false)) {}

  ScopeGuard(const ScopeGuard&) = delete;
  auto operator=(const ScopeGuard&) -> ScopeGuard& = delete;
  auto operator=(ScopeGuard&&) -> ScopeGuard& = delete;

  ~ScopeGuard() {
    if (active_)
      cleanup_();
  }

  auto dismiss() noexcept -> void { active_ = false; }

private:
  F cleanup_;
  bool active_ = true;
};

} // namespace kitzoo::core

#endif // KITZOO_CORE_SCOPE_GUARD_HPP
