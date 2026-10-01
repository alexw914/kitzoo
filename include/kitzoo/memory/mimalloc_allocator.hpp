// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/mimalloc_allocator.hpp
// Description: Adapts mimalloc allocation to the C++ allocator interface for
//              standard-library containers.
// -----------------------------------------------------------------------------

#pragma once

#if !defined(KZ_WITH_MIMALLOC)
#error "kitzoo/memory/mimalloc_allocator.hpp requires the kitzoo::memory target"
#endif

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <limits>
#include <mimalloc.h>
#include <new>

namespace kitzoo::memory {

template <typename T>
class MimallocAllocator {
public:
    using value_type = T;

    MimallocAllocator() noexcept = default;
    template <typename U>
    constexpr MimallocAllocator(MimallocAllocator<U> const&) noexcept {}

    KZ_NODISCARD auto allocate(std::size_t count) -> T* {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length{};
        auto* memory = mi_malloc_aligned(count * sizeof(T), alignof(T));
        if (memory == nullptr)
            throw std::bad_alloc{};
        return static_cast<T*>(memory);
    }
    auto deallocate(T* memory, std::size_t) noexcept -> void { mi_free(memory); }

    template <typename U>
    struct rebind {
        using other = MimallocAllocator<U>;
    };
};

template <typename T, typename U>
constexpr auto operator==(MimallocAllocator<T> const&, MimallocAllocator<U> const&) noexcept
    -> bool {
    return true;
}

}  // namespace kitzoo::memory
