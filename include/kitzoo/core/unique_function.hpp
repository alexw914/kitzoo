// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/unique_function.hpp
// Description: Declares a move-only type-erased callable wrapper for storing
//              non-copyable callables.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_UNIQUE_FUNCTION_HPP
#define KITZOO_CORE_UNIQUE_FUNCTION_HPP

#include <kitzoo/core/macro.hpp>

#include <cassert>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace kitzoo::core {

template <typename Signature>
class unique_function;

template <typename R, typename... Args>
class unique_function<R(Args...)> {
  static constexpr std::size_t kSboSize = 3 * sizeof(void*);

  struct VTable {
    R (*invoke)(void* obj, Args&&... args);
    void* (*move_to)(void* from, void* to) noexcept;
    void (*destroy)(void* obj) noexcept;
  };

public:
  unique_function() noexcept = default;

  unique_function(std::nullptr_t) noexcept {}

  // A null function or member pointer produces an empty wrapper.
  template <typename F>
    requires(!std::is_same_v<std::decay_t<F>, unique_function> && std::is_invocable_r_v<R, std::decay_t<F>&, Args...>)
  unique_function(F&& f) {
    using T = std::decay_t<F>;
    if constexpr (std::is_pointer_v<T> || std::is_member_pointer_v<T>) {
      if (f == nullptr)
        return;
    }
    if constexpr (sizeof(T) <= kSboSize && alignof(T) <= alignof(void*) && std::is_nothrow_move_constructible_v<T>) {
      obj_ = ::new (static_cast<void*>(storage_)) T(std::forward<F>(f));
      vtable_ = &vtable_for<T, true>();
    } else {
      obj_ = new T(std::forward<F>(f));
      vtable_ = &vtable_for<T, false>();
    }
  }

  ~unique_function() { reset(); }

  unique_function(unique_function&& other) noexcept { move_from(std::move(other)); }

  auto operator=(unique_function&& other) noexcept -> unique_function& {
    if (this != &other) {
      reset();
      move_from(std::move(other));
    }
    return *this;
  }

  unique_function(const unique_function&) = delete;
  auto operator=(const unique_function&) -> unique_function& = delete;

  KZ_NODISCARD explicit operator bool() const noexcept { return vtable_ != nullptr; }

  auto operator()(Args... args) -> R {
    assert(vtable_ && "unique_function called while empty");
    return vtable_->invoke(obj_, std::forward<Args>(args)...);
  }

private:
  auto reset() noexcept -> void {
    if (vtable_) {
      vtable_->destroy(obj_);
      vtable_ = nullptr;
      obj_ = nullptr;
    }
  }

  auto move_from(unique_function&& other) noexcept -> void {
    if (!other.vtable_)
      return;
    vtable_ = other.vtable_;
    if (other.in_sbo()) {
      obj_ = vtable_->move_to(other.obj_, storage_);
    } else {
      obj_ = other.obj_;
    }
    other.vtable_ = nullptr;
    other.obj_ = nullptr;
  }

  KZ_NODISCARD auto in_sbo() const noexcept -> bool { return obj_ == static_cast<const void*>(storage_); }

  template <typename T, bool InSbo>
  static auto vtable_for() -> const VTable& {
    static const VTable table{

        [](void* obj, Args&&... args) -> R {
          if constexpr (std::is_void_v<R>)
            std::invoke(*static_cast<T*>(obj), std::forward<Args>(args)...);
          else
            return std::invoke(*static_cast<T*>(obj), std::forward<Args>(args)...);
        },

        [](KZ_MAYBE_UNUSED void* from, KZ_MAYBE_UNUSED void* to) noexcept -> void* {
          if constexpr (InSbo) {
            auto* moved = ::new (to) T(std::move(*static_cast<T*>(from)));
            static_cast<T*>(from)->~T();
            return moved;
          } else {
            return from;
          }
        },

        [](void* obj) noexcept {
          if constexpr (InSbo) {
            static_cast<T*>(obj)->~T();
          } else {
            delete static_cast<T*>(obj);
          }
        },
    };
    return table;
  }

  alignas(void*) std::byte storage_[kSboSize];
  void* obj_ = nullptr;
  const VTable* vtable_ = nullptr;
};

} // namespace kitzoo::core

#endif // KITZOO_CORE_UNIQUE_FUNCTION_HPP
