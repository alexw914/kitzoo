// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities/singleton.hpp
// Description: Declares lazy and eager singleton wrappers with one instance per
//              singleton type.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

namespace kitzoo::util {

template <typename T>
class Singleton {
public:
    Singleton() = delete;

    KZ_NODISCARD static auto instance() -> T& {
        static T value;
        return value;
    }
};

template <typename T>
class EagerSingleton {
public:
    EagerSingleton() = delete;

    KZ_NODISCARD static auto instance() noexcept -> T& { return value_; }

private:
    inline static T value_{};
};

}  // namespace kitzoo::util
