// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/singleton.hpp
// Description: Declares lazy and eager singleton wrappers with thread-safe
//              initialization of one instance per singleton type.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_SINGLETON_HPP
#define KITZOO_UTILITIES_SINGLETON_HPP

#include <kitzoo/core/macro.hpp>

namespace kitzoo::util {

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

template <typename T>
class EagerSingleton {
public:
    EagerSingleton(const EagerSingleton&) = delete;
    EagerSingleton(EagerSingleton&&) = delete;
    auto operator=(const EagerSingleton&) -> EagerSingleton& = delete;
    auto operator=(EagerSingleton&&) -> EagerSingleton& = delete;

    KZ_NODISCARD static auto instance() -> T& {
        // ODR-use the startup anchor without reading it during dynamic initialization.
        static_cast<void>(&value_);
        return storage();
    }

protected:
    EagerSingleton() = default;
    ~EagerSingleton() = default;

private:
    static auto storage() -> T& {
        // Startup initialization and concurrent callers share the same guarded object.
        static T value;
        return value;
    }

    // Request eager initialization; instance() is also safe if initialization is deferred.
    inline static T* value_ = &storage();
};

}  // namespace kitzoo::util

#endif  // KITZOO_UTILITIES_SINGLETON_HPP
