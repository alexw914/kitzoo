// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/memory/basicmemory.cpp
// Description: Implements mimalloc-backed pools and native shared mappings.
// -----------------------------------------------------------------------------

#include <kitzoo/core/scopeguard.hpp>
#include <kitzoo/memory/basicmemory.hpp>

#include <cstdint>
#include <iterator>
#include <limits>
#include <mimalloc.h>
#include <mutex>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace kitzoo::memory {
namespace {

constexpr std::size_t kBlockSize = 16;

auto valid_alignment(std::size_t alignment) -> bool {
  return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

struct SharedPool {
  void* storage = nullptr;
  std::size_t bytes = 0;
  // Address-ordered, coalesced free extents: first block -> block count.
  Map<std::size_t, std::size_t> free_extents;
  // Allocation start block -> requested byte size.
  Map<std::size_t, std::size_t> allocations;

  explicit SharedPool(std::size_t size) : bytes(size) {
    if (size / kBlockSize != 0)
      free_extents.emplace(0, size / kBlockSize);
  }

  auto contains(const void* address) const -> bool {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(storage);
    return storage != nullptr && value >= base && value - base < bytes;
  }

  static auto block_count(std::size_t size) -> std::size_t { return size / kBlockSize + (size % kBlockSize != 0); }

  // First fit over free extents; mappings are page-aligned, so block starts are 16-byte aligned.
  auto allocate(std::size_t size, std::size_t alignment) -> void* {
    if (size == 0 || size > bytes || !valid_alignment(alignment))
      return nullptr;
    const auto count = block_count(size);
    const auto base = reinterpret_cast<std::uintptr_t>(storage);
    for (auto it = free_extents.begin(); it != free_extents.end(); ++it) {
      const auto [start, length] = *it;
      const auto address = base + start * kBlockSize;
      const auto aligned = (address + alignment - 1) / alignment * alignment;
      if ((aligned - base) % kBlockSize != 0)
        continue;
      const auto index = (aligned - base) / kBlockSize;
      if (index + count > start + length)
        continue;
      free_extents.erase(it);
      if (index > start)
        free_extents.emplace(start, index - start);
      if (index + count < start + length)
        free_extents.emplace(index + count, start + length - index - count);
      allocations.emplace(index, size);
      return static_cast<unsigned char*>(storage) + index * kBlockSize;
    }
    return nullptr;
  }

  auto deallocate(void* address, std::size_t size) -> void {
    const auto offset = reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(storage);
    if (offset % kBlockSize != 0)
      return;
    const auto found = allocations.find(offset / kBlockSize);
    if (found == allocations.end() || found->second != size)
      return;
    auto start = found->first;
    auto length = block_count(size);
    allocations.erase(found);
    const auto next = free_extents.lower_bound(start);
    if (next != free_extents.end() && start + length == next->first) {
      length += next->second;
      free_extents.erase(next);
    }
    const auto after = free_extents.lower_bound(start);
    if (after != free_extents.begin()) {
      const auto previous = std::prev(after);
      if (previous->first + previous->second == start) {
        start = previous->first;
        length += previous->second;
        free_extents.erase(previous);
      }
    }
    free_extents.emplace(start, length);
  }
};

} // namespace

namespace {

class BasicResource final : public std::pmr::memory_resource {
public:
  BasicResource(BasicMemory& owner, bool shared) : owner_(owner), shared_(shared) {}

private:
  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
    const auto actual_bytes = bytes == 0 ? 1 : bytes;
    auto* address = shared_ ? owner_.allocate_shared(actual_bytes, nullptr, alignment)
                            : owner_.allocate(actual_bytes, nullptr, alignment);
    if (!address)
      throw std::bad_alloc{};
    return address;
  }

  auto do_deallocate(void* address, std::size_t bytes, std::size_t) -> void override {
    owner_.deallocate(address, bytes == 0 ? 1 : bytes);
  }

  auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override { return this == &other; }

  BasicMemory& owner_;
  bool shared_;
};

} // namespace

struct BasicMemory::Impl {
  BasicResource ordinary_resource;
  BasicResource shared_resource;

  explicit Impl(BasicMemory& owner) : ordinary_resource(owner, false), shared_resource(owner, true) {}

  mutable std::mutex mutex;
  UniquePtr<LimitedResource> virtual_budget;
  UniquePtr<SharedPool> shared_pool;
  String shared_name;
#if defined(_WIN32)
  HANDLE mapping = nullptr;
#endif

  ~Impl() {
    if (shared_pool) {
#if defined(_WIN32)
      UnmapViewOfFile(shared_pool->storage);
      CloseHandle(mapping);
#else
      munmap(shared_pool->storage, shared_pool->bytes);
      shm_unlink(shared_name.c_str());
#endif
    }
  }

  auto allocate_virtual(std::size_t size, std::size_t alignment) -> void* {
    return virtual_budget ? virtual_budget->try_allocate(size, alignment) : mi_malloc_aligned(size, alignment);
  }
};

BasicMemory::BasicMemory() : impl_(memory::make_unique<Impl>(*this)) {}

BasicMemory::~BasicMemory() = default;

auto BasicMemory::init_virtual(const BasicMemoryConfig& config) -> bool {
  std::lock_guard lock(impl_->mutex);
  if (impl_->virtual_budget || config.memory_size_bytes == 0)
    return false;
  impl_->virtual_budget = memory::make_unique<LimitedResource>(config.memory_size_bytes);
  return true;
}

auto BasicMemory::init_shared(const SharedBasicMemoryConfig& config) -> bool {
  std::lock_guard lock(impl_->mutex);
  const auto& name = config.shared_memory_name;
  if (impl_->shared_pool || config.memory_size_bytes < kBlockSize || name.empty() ||
      name.find('\0') != std::string::npos)
    return false;
  auto pool = memory::make_unique<SharedPool>(config.memory_size_bytes);
  // Prepare all throwing allocations before acquiring OS resources.
  String owned_name{name.data(), name.size()};
#if defined(_WIN32)
  const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.c_str(), -1, nullptr, 0);
  if (length <= 0)
    return false;
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.c_str(), -1, wide.data(), length);
  const auto bytes = static_cast<std::uint64_t>(config.memory_size_bytes);
  auto mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, static_cast<DWORD>(bytes >> 32),
                                    static_cast<DWORD>(bytes), wide.c_str());
  if (!mapping)
    return false;
  const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;
  core::ScopeGuard close_mapping{[&]() noexcept { CloseHandle(mapping); }};
  if (existed)
    return false;
  pool->storage = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, config.memory_size_bytes);
  if (!pool->storage)
    return false;
  close_mapping.dismiss();
  impl_->mapping = mapping;
#else
  if (name.front() != '/' || name.size() == 1 || name.find('/', 1) != std::string::npos ||
      config.memory_size_bytes > static_cast<std::uintmax_t>(std::numeric_limits<off_t>::max()))
    return false;
  const auto fd = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    return false;
  // The mapping stays valid after its descriptor closes.
  KZ_SCOPE_EXIT {
    close(fd);
  };
  core::ScopeGuard unlink{[&]() noexcept { shm_unlink(name.c_str()); }};
  if (ftruncate(fd, static_cast<off_t>(config.memory_size_bytes)) != 0)
    return false;
  auto* storage = mmap(nullptr, config.memory_size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (storage == MAP_FAILED)
    return false;
  unlink.dismiss();
  pool->storage = storage;
#endif
  impl_->shared_name = std::move(owned_name);
  impl_->shared_pool = std::move(pool);
  return true;
}

auto BasicMemory::memory_type(const void* address) const -> MemoryType {
  std::lock_guard lock(impl_->mutex);
  if (impl_->virtual_budget && impl_->virtual_budget->owns(address))
    return MemoryType::Virtual;
  if (impl_->shared_pool && impl_->shared_pool->contains(address))
    return MemoryType::Shared;
  return MemoryType::Unknown;
}

auto BasicMemory::allocate(std::size_t size, const void*, std::size_t alignment) -> void* {
  if (size == 0 || !valid_alignment(alignment))
    return nullptr;
  std::lock_guard lock(impl_->mutex);
  return impl_->allocate_virtual(size, alignment);
}

auto BasicMemory::allocate_shared(std::size_t size, const void*, std::size_t alignment) -> void* {
  if (size == 0 || !valid_alignment(alignment))
    return nullptr;
  std::lock_guard lock(impl_->mutex);
  if (impl_->shared_pool) {
    if (auto* address = impl_->shared_pool->allocate(size, alignment))
      return address;
  }
  return impl_->allocate_virtual(size, alignment);
}

auto BasicMemory::deallocate(void* address, std::size_t size) -> void {
  if (!address)
    return;
  std::lock_guard lock(impl_->mutex);
  if (impl_->virtual_budget && impl_->virtual_budget->owns(address))
    impl_->virtual_budget->release(address, size);
  else if (impl_->shared_pool && impl_->shared_pool->contains(address))
    impl_->shared_pool->deallocate(address, size);
  else
    mi_free(address);
}

auto BasicMemory::resource() noexcept -> std::pmr::memory_resource* {
  return &impl_->ordinary_resource;
}

auto BasicMemory::shared_resource() noexcept -> std::pmr::memory_resource* {
  return &impl_->shared_resource;
}

auto BasicMemory::virtual_stats() const -> MemoryStats {
  std::lock_guard lock(impl_->mutex);
  return impl_->virtual_budget ? impl_->virtual_budget->stats() : MemoryStats{};
}

auto BasicMemory::shared_memory_name() const -> std::string {
  std::lock_guard lock(impl_->mutex);
  return {impl_->shared_name.data(), impl_->shared_name.size()};
}

auto BasicMemory::shared_memory_size_bytes() const -> std::size_t {
  std::lock_guard lock(impl_->mutex);
  return impl_->shared_pool ? impl_->shared_pool->bytes : 0;
}

auto BasicMemory::shared_memory_address() const -> void* {
  std::lock_guard lock(impl_->mutex);
  return impl_->shared_pool ? impl_->shared_pool->storage : nullptr;
}

} // namespace kitzoo::memory
