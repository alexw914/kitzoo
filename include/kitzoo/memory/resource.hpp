// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/resource.hpp
// Description: Provides mimalloc and bounded PMR resources with allocation statistics.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_RESOURCE_HPP
#define KITZOO_MEMORY_RESOURCE_HPP

#include <kitzoo/memory/memory.hpp>

#include <cstddef>
#include <memory>
#include <memory_resource>

namespace kitzoo::memory {

struct MemoryStats {
  std::size_t used_bytes = 0;
  std::size_t peak_bytes = 0;
  std::size_t allocation_count = 0;
};

auto mimalloc_resource() -> std::pmr::memory_resource*;

// Limits live requested bytes, not RSS or allocator overhead. The upstream
// resource must outlive this resource. Free all allocations before destruction.
class LimitedResource final : public std::pmr::memory_resource {
public:
  explicit LimitedResource(std::size_t capacity, std::pmr::memory_resource* upstream = mimalloc_resource());

  ~LimitedResource() override;

  LimitedResource(const LimitedResource&) = delete;
  LimitedResource(LimitedResource&&) = delete;
  auto operator=(const LimitedResource&) -> LimitedResource& = delete;
  auto operator=(LimitedResource&&) -> LimitedResource& = delete;

  auto capacity() const noexcept -> std::size_t;

  auto stats() const -> MemoryStats;

  auto owns(const void* address) const -> bool;

  auto try_allocate(std::size_t bytes, std::size_t alignment) -> void*;

  // Legacy release without alignment; mismatched sizes/foreign pointers fail.
  auto release(void* address, std::size_t bytes) -> bool;

private:
  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override;

  auto do_deallocate(void* address, std::size_t bytes, std::size_t alignment) -> void override;

  auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override;

  struct Impl;
  UniquePtr<Impl> impl_;
};

} // namespace kitzoo::memory

#endif // KITZOO_MEMORY_RESOURCE_HPP
