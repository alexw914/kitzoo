// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/memory.hpp
// Description: Provides resource-selectable shared ownership for objects and arrays.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_MEMORY_HPP
#define KITZOO_MEMORY_MEMORY_HPP

#include <kitzoo/memory/advanced_types.hpp>

#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace kitzoo::memory {

class Memory {
public:
  explicit Memory(std::pmr::memory_resource* resource = nullptr) noexcept : resource_(resource) {}

  template <typename T, typename... Args>
  auto alloc_object(Args&&... args) -> SharedPtr<T> {
    if (resource_)
      return memory::make_shared_with_resource<T>(resource_, std::forward<Args>(args)...);
    return memory::make_shared<T>(std::forward<Args>(args)...);
  }

  // Value-initializes elements and destroys each one before freeing storage.
  template <typename T>
  auto alloc_basic_array(std::size_t count) -> SharedPtr<T[]> {
    if (count == 0)
      return {};
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length{};
    if (resource_)
      return alloc_array<T>(std::pmr::polymorphic_allocator<T>{resource_}, count);
    return alloc_array<T>(MiAllocator<T>{}, count);
  }

private:
  template <typename T, typename Allocator>
  static auto alloc_array(Allocator allocator, std::size_t count) -> SharedPtr<T[]> {
    using Traits = std::allocator_traits<Allocator>;
    auto* address = allocator.allocate(count);
    std::size_t constructed = 0;
    try {
      for (; constructed < count; ++constructed)
        Traits::construct(allocator, address + constructed);
    } catch (...) {
      std::destroy_n(address, constructed);
      allocator.deallocate(address, count);
      throw;
    }
    // shared_ptr invokes the deleter if allocating its control block fails.
    return SharedPtr<T[]>(
        address,
        [count, allocator](T* pointer) mutable -> void {
          std::destroy_n(pointer, count);
          allocator.deallocate(pointer, count);
        },
        allocator);
  }

  std::pmr::memory_resource* resource_;
};

} // namespace kitzoo::memory

#endif // KITZOO_MEMORY_MEMORY_HPP
