// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/memory/basic_memory.cpp
// Description: Implements mimalloc-backed pools and native shared mappings.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/basic_memory.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <mimalloc.h>
#include <mutex>
#include <vector>

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
  std::size_t cursor = 0;
  // Allocation starts store byte sizes; continuation blocks store a sentinel.
  std::vector<std::size_t> blocks;

  explicit SharedPool(std::size_t size) : bytes(size), blocks(size / kBlockSize, 0) {}

  auto contains(void const* address) const -> bool {
    auto const value = reinterpret_cast<std::uintptr_t>(address);
    auto const base = reinterpret_cast<std::uintptr_t>(storage);
    return storage != nullptr && value >= base && value - base < bytes;
  }

  auto allocate(std::size_t size, std::size_t alignment) -> void* {
    if (size == 0 || size > blocks.size() * kBlockSize || !valid_alignment(alignment))
      return nullptr;
    auto const count = size / kBlockSize + (size % kBlockSize != 0 ? 1U : 0U);
    auto const base = reinterpret_cast<std::uintptr_t>(storage);
    for (std::size_t scanned = 0; scanned < blocks.size(); ++scanned) {
      auto const index = (cursor + scanned) % blocks.size();
      if (count > blocks.size() - index || (base + index * kBlockSize) % alignment != 0)
        continue;
      auto first = blocks.begin() + static_cast<std::ptrdiff_t>(index);
      auto last = first + static_cast<std::ptrdiff_t>(count);
      if (!std::all_of(first, last, [](std::size_t block) -> bool { return block == 0; }))
        continue;
      std::fill(first, last, std::numeric_limits<std::size_t>::max());
      *first = size;
      cursor = (index + count) % blocks.size();
      return static_cast<unsigned char*>(storage) + index * kBlockSize;
    }
    return nullptr;
  }

  auto deallocate(void* address, std::size_t size) -> void {
    auto const offset = reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(storage);
    if (offset % kBlockSize != 0 || offset / kBlockSize >= blocks.size() || size == 0)
      return;
    auto const index = offset / kBlockSize;
    if (blocks[index] != size || size == std::numeric_limits<std::size_t>::max())
      return;
    auto const count = size / kBlockSize + (size % kBlockSize != 0 ? 1U : 0U);
    std::fill_n(blocks.begin() + static_cast<std::ptrdiff_t>(index), count, 0);
  }
};

} // namespace

namespace {

class BasicResource final : public std::pmr::memory_resource {
public:
  BasicResource(BasicMemory& owner, bool shared) : owner_(owner), shared_(shared) {}

private:
  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
    auto const actual_bytes = bytes == 0 ? 1 : bytes;
    auto* address = shared_ ? owner_.allocate_shared(actual_bytes, nullptr, alignment)
                            : owner_.allocate(actual_bytes, nullptr, alignment);
    if (!address)
      throw std::bad_alloc{};
    return address;
  }

  auto do_deallocate(void* address, std::size_t bytes, std::size_t) -> void override {
    owner_.deallocate(address, bytes == 0 ? 1 : bytes);
  }

  auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override { return this == &other; }

  BasicMemory& owner_;
  bool shared_;
};

} // namespace

struct BasicMemory::Impl {
  BasicResource ordinary_resource;
  BasicResource shared_resource;

  explicit Impl(BasicMemory& owner) : ordinary_resource(owner, false), shared_resource(owner, true) {}

  mutable std::mutex mutex;
  std::unique_ptr<LimitedResource> virtual_budget;
  std::unique_ptr<SharedPool> shared_pool;
  std::string shared_name;
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

BasicMemory::BasicMemory() : impl_(std::make_unique<Impl>(*this)) {}

BasicMemory::~BasicMemory() = default;

auto BasicMemory::init_virtual(BasicMemoryConfig const& config) -> bool {
  std::lock_guard lock(impl_->mutex);
  if (impl_->virtual_budget || config.memory_size_bytes == 0)
    return false;
  impl_->virtual_budget = std::make_unique<LimitedResource>(config.memory_size_bytes);
  return true;
}

auto BasicMemory::init_shared(SharedBasicMemoryConfig const& config) -> bool {
  std::lock_guard lock(impl_->mutex);
  auto const& name = config.shared_memory_name;
  if (impl_->shared_pool || config.memory_size_bytes < kBlockSize || name.empty() ||
      name.find('\0') != std::string::npos)
    return false;
  auto pool = std::make_unique<SharedPool>(config.memory_size_bytes);
  // Prepare all throwing allocations before acquiring OS resources.
  auto owned_name = name;
#if defined(_WIN32)
  auto const length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.c_str(), -1, nullptr, 0);
  if (length <= 0)
    return false;
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.c_str(), -1, wide.data(), length);
  auto const bytes = static_cast<std::uint64_t>(config.memory_size_bytes);
  auto mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, static_cast<DWORD>(bytes >> 32),
                                    static_cast<DWORD>(bytes), wide.c_str());
  if (!mapping)
    return false;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mapping);
    return false;
  }
  pool->storage = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, config.memory_size_bytes);
  if (!pool->storage) {
    CloseHandle(mapping);
    return false;
  }
  impl_->mapping = mapping;
#else
  if (name.front() != '/' || name.size() == 1 || name.find('/', 1) != std::string::npos ||
      config.memory_size_bytes > static_cast<std::uintmax_t>(std::numeric_limits<off_t>::max()))
    return false;
  auto const fd = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    return false;
  if (ftruncate(fd, static_cast<off_t>(config.memory_size_bytes)) != 0) {
    close(fd);
    shm_unlink(name.c_str());
    return false;
  }
  auto* storage = mmap(nullptr, config.memory_size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (storage == MAP_FAILED) {
    shm_unlink(name.c_str());
    return false;
  }
  pool->storage = storage;
#endif
  impl_->shared_name = std::move(owned_name);
  impl_->shared_pool = std::move(pool);
  return true;
}

auto BasicMemory::memory_type(void const* address) const -> MemoryType {
  std::lock_guard lock(impl_->mutex);
  if (impl_->virtual_budget && impl_->virtual_budget->owns(address))
    return MemoryType::Virtual;
  if (impl_->shared_pool && impl_->shared_pool->contains(address))
    return MemoryType::Shared;
  return MemoryType::Unknown;
}

auto BasicMemory::allocate(std::size_t size, void const*, std::size_t alignment) -> void* {
  if (size == 0 || !valid_alignment(alignment))
    return nullptr;
  std::lock_guard lock(impl_->mutex);
  return impl_->allocate_virtual(size, alignment);
}

auto BasicMemory::allocate_shared(std::size_t size, void const*, std::size_t alignment) -> void* {
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

auto BasicMemory::get_shared_memory_name() const -> std::string {
  std::lock_guard lock(impl_->mutex);
  return impl_->shared_name;
}

auto BasicMemory::get_memory_size_bytes() const -> std::size_t {
  std::lock_guard lock(impl_->mutex);
  return impl_->shared_pool ? impl_->shared_pool->bytes : 0;
}

auto BasicMemory::get_shared_memory_start_address() const -> void* {
  std::lock_guard lock(impl_->mutex);
  return impl_->shared_pool ? impl_->shared_pool->storage : nullptr;
}

} // namespace kitzoo::memory
