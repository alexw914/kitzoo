// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/memory/resource.cpp
// Description: Implements aligned mimalloc allocation and synchronized allocation budgets.
// -----------------------------------------------------------------------------

#include <kitzoo/core/scope_guard.hpp>
#include <kitzoo/memory/resource.hpp>

#include <mimalloc.h>
#include <mutex>
#include <new>
#include <stdexcept>

namespace kitzoo::memory {
namespace {

class MimallocResource final : public std::pmr::memory_resource {
  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
      throw std::bad_alloc{};
    auto* address = mi_malloc_aligned(bytes == 0 ? 1 : bytes, alignment);
    if (!address)
      throw std::bad_alloc{};
    return address;
  }

  auto do_deallocate(void* address, std::size_t, std::size_t) -> void override { mi_free(address); }

  auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override { return this == &other; }
};

} // namespace

auto mimalloc_resource() -> std::pmr::memory_resource* {
  static MimallocResource resource;
  return &resource;
}

struct LimitedResource::Impl {
  struct Allocation {
    std::size_t bytes;
    std::size_t alignment;
  };

  std::size_t capacity;
  std::pmr::memory_resource* upstream;
  mutable std::mutex mutex;
  MemoryStats stats;
  UnorderedMap<const void*, Allocation> allocations;

  Impl(std::size_t limit, std::pmr::memory_resource* resource) : capacity(limit), upstream(resource) {}
};

LimitedResource::LimitedResource(std::size_t capacity, std::pmr::memory_resource* upstream)
    : impl_(memory::make_unique<Impl>(capacity, upstream)) {
  if (!upstream)
    throw std::invalid_argument("LimitedResource requires an upstream resource");
}

LimitedResource::~LimitedResource() = default;

auto LimitedResource::capacity() const noexcept -> std::size_t {
  return impl_->capacity;
}

auto LimitedResource::stats() const -> MemoryStats {
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

auto LimitedResource::owns(const void* address) const -> bool {
  std::lock_guard lock(impl_->mutex);
  return impl_->allocations.contains(address);
}

auto LimitedResource::try_allocate(std::size_t bytes, std::size_t alignment) -> void* {
  return allocate_within_budget(bytes, alignment);
}

auto LimitedResource::do_allocate(std::size_t bytes, std::size_t alignment) -> void* {
  if (auto* address = allocate_within_budget(bytes, alignment))
    return address;
  throw std::bad_alloc{};
}

auto LimitedResource::allocate_within_budget(std::size_t bytes, std::size_t alignment) -> void* {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    return nullptr;
  std::lock_guard lock(impl_->mutex);
  if (bytes > impl_->capacity - impl_->stats.used_bytes)
    return nullptr;
  auto* address = impl_->upstream->allocate(bytes, alignment);
  core::ScopeGuard release{[&] { impl_->upstream->deallocate(address, bytes, alignment); }};
  impl_->allocations.emplace(address, Impl::Allocation{bytes, alignment});
  release.dismiss();
  impl_->stats.used_bytes += bytes;
  if (impl_->stats.used_bytes > impl_->stats.peak_bytes)
    impl_->stats.peak_bytes = impl_->stats.used_bytes;
  ++impl_->stats.allocation_count;
  return address;
}

auto LimitedResource::release(void* address, std::size_t bytes) -> bool {
  std::lock_guard lock(impl_->mutex);
  const auto found = impl_->allocations.find(address);
  if (found == impl_->allocations.end() || found->second.bytes != bytes)
    return false;
  impl_->upstream->deallocate(address, bytes, found->second.alignment);
  impl_->allocations.erase(found);
  impl_->stats.used_bytes -= bytes;
  --impl_->stats.allocation_count;
  return true;
}

auto LimitedResource::do_deallocate(void* address, std::size_t bytes, std::size_t alignment) -> void {
  std::lock_guard lock(impl_->mutex);
  const auto found = impl_->allocations.find(address);
  if (found == impl_->allocations.end() || found->second.bytes != bytes || found->second.alignment != alignment)
    return;
  impl_->upstream->deallocate(address, bytes, alignment);
  impl_->allocations.erase(found);
  impl_->stats.used_bytes -= bytes;
  --impl_->stats.allocation_count;
}

auto LimitedResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool {
  return this == &other;
}

} // namespace kitzoo::memory
