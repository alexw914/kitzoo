// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/singleton.hpp
// Description: Declares lazy and eager singleton wrappers with thread-safe
//              initialization of one instance per singleton type.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_SINGLETON_HPP
#define KITZOO_CORE_SINGLETON_HPP

#include <kitzoo/core/macro.hpp>

namespace kitzoo::core {

template <typename T>
class Singleton {
public:
  Singleton(const Singleton&) = delete;
  Singleton(Singleton&&) = delete;
  auto operator=(const Singleton&) -> Singleton& = delete;
  auto operator=(Singleton&&) -> Singleton& = delete;

  KZ_NODISCARD static auto instance() -> T& {
    // C++ guarantees serialized initialization and retries after a constructor throws.
    static T value;
    return value;
  }

protected:
  Singleton() = default;
  ~Singleton() = default;
};

// Requests initialization during static startup. C++ may defer that initialization;
// early and concurrent callers still receive the same fully constructed object.
template <typename T>
class EagerSingleton {
public:
  EagerSingleton(const EagerSingleton&) = delete;
  EagerSingleton(EagerSingleton&&) = delete;
  auto operator=(const EagerSingleton&) -> EagerSingleton& = delete;
  auto operator=(EagerSingleton&&) -> EagerSingleton& = delete;

  KZ_NODISCARD static auto instance() -> T& {
    // ODR-use the initializer without reading its value during static startup.
    static_cast<void>(&value_);
    return storage();
  }

protected:
  EagerSingleton() = default;
  ~EagerSingleton() = default;

private:
  static auto storage() -> T& {
    // Static startup and concurrent access use the same initialization guard.
    static T value;
    return value;
  }

  inline static T* value_ = &storage();
};

} // namespace kitzoo::core

#endif // KITZOO_CORE_SINGLETON_HPP
