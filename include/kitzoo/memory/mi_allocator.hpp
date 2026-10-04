// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/mi_allocator.hpp
// Description: Provides a stateless standard allocator that calls mimalloc directly.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_MI_ALLOCATOR_HPP
#define KITZOO_MEMORY_MI_ALLOCATOR_HPP

#include <cstddef>
#include <limits>
#include <mimalloc.h>
#include <new>

namespace kitzoo::memory {

template <typename T>
class MiAllocator {
public:
    using value_type = T;

    MiAllocator() noexcept = default;

    template <typename U>
    constexpr MiAllocator(MiAllocator<U> const&) noexcept {}

    auto allocate(std::size_t count) -> T* {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length{};
        auto const bytes = count == 0 ? sizeof(T) : count * sizeof(T);
        void* address;
        // Ordinary types take the normal mimalloc path. Over-aligned types
        // select the aligned API at compile time, with no runtime resource branch.
        if constexpr (alignof(T) > alignof(std::max_align_t))
            address = mi_malloc_aligned(bytes, alignof(T));
        else
            address = mi_malloc(bytes);
        if (!address)
            throw std::bad_alloc{};
        return static_cast<T*>(address);
    }

    auto deallocate(T* address, std::size_t) noexcept -> void { mi_free(address); }

    template <typename U>
    struct rebind {
        using other = MiAllocator<U>;
    };
};

template <typename T, typename U>
constexpr auto operator==(MiAllocator<T> const&, MiAllocator<U> const&) noexcept -> bool {
    return true;
}

}  // namespace kitzoo::memory

#endif  // KITZOO_MEMORY_MI_ALLOCATOR_HPP
