// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/singleton.hpp
// Description: Declares lazy and eager singleton wrappers with one instance per
//              singleton type.
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

    KZ_NODISCARD static auto instance() noexcept -> T& { return value_; }

protected:
    EagerSingleton() = default;
    ~EagerSingleton() = default;

private:
    inline static T value_{};
};

}  // namespace kitzoo::util

#endif  // KITZOO_UTILITIES_SINGLETON_HPP
