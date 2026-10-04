// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/scope_guard.hpp
// Description: Declares a move-only scope guard that runs a cleanup callable
//              unless dismissed.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_SCOPE_GUARD_HPP
#define KITZOO_UTILITIES_SCOPE_GUARD_HPP

#include <kitzoo/core/macro.hpp>

#include <type_traits>
#include <utility>

namespace kitzoo::util {

template <typename F>
class ScopeGuard {
public:
    explicit ScopeGuard(F f) noexcept(std::is_nothrow_move_constructible_v<F>) : f_(std::move(f)) {}

    ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : f_(std::move(other.f_)), active_{other.active_} {
        other.dismiss();
    }

    ScopeGuard(ScopeGuard const&) = delete;
    auto operator=(ScopeGuard const&) -> ScopeGuard& = delete;
    auto operator=(ScopeGuard&&) -> ScopeGuard& = delete;

    ~ScopeGuard() noexcept {
        if (active_)
            f_();
    }

    auto dismiss() noexcept -> void { active_ = false; }

    KZ_NODISCARD auto active() const noexcept -> bool { return active_; }

private:
    F f_;
    bool active_{true};
};

template <typename F>
ScopeGuard(F) -> ScopeGuard<F>;

template <typename F>
KZ_NODISCARD auto make_scope_guard(F&& f) -> ScopeGuard<std::decay_t<F>> {
    return ScopeGuard<std::decay_t<F>>(std::forward<F>(f));
}

}  // namespace kitzoo::util

#endif  // KITZOO_UTILITIES_SCOPE_GUARD_HPP
