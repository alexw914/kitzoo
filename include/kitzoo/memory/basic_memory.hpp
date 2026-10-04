// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/basic_memory.hpp
// Description: Declares the default allocation budget and native shared mapping facade.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_BASIC_MEMORY_HPP
#define KITZOO_MEMORY_BASIC_MEMORY_HPP

#include <kitzoo/memory/resource.hpp>
#include <kitzoo/utilities/singleton.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace kitzoo::memory {

struct BasicMemoryConfig {
    std::size_t memory_size_bytes = 0;
};

struct SharedBasicMemoryConfig {
    std::string shared_memory_name;
    std::size_t memory_size_bytes = 0;
};

enum class MemoryType {
    Virtual,
    Shared,
    Unknown,
};

class BasicMemory : public util::Singleton<BasicMemory> {
public:
    ~BasicMemory();

    // One-shot logical byte limit; allocations remain backed by mimalloc.
    auto init_virtual(BasicMemoryConfig const& config) -> bool;

    // Creates exclusively; never removes or replaces an existing named mapping.
    // POSIX names must start with '/' and contain no other '/'.
    auto init_shared(SharedBasicMemoryConfig const& config) -> bool;

    auto memory_type(void const* address) const -> MemoryType;

    // Stable PMR adapters. Dependent objects must not outlive this singleton.
    auto resource() noexcept -> std::pmr::memory_resource*;

    // Uses the shared mapping first, with the same fallback as allocate_shared().
    auto shared_resource() noexcept -> std::pmr::memory_resource*;

    auto virtual_stats() const -> MemoryStats;

    // Zero or invalid alignment returns nullptr. Exhausted virtual budgets return
    // nullptr; before virtual initialization allocations use mimalloc directly.
    // prefix_address is retained as a hint for interface parity; currently ignored.
    auto allocate(std::size_t size, void const* prefix_address = nullptr,
                  std::size_t alignment = 16) -> void*;

    // Tries shared pool first, then allocate(). Pool metadata is process-local:
    // only the creating process may allocate/free; mapped readers use offsets.
    auto allocate_shared(std::size_t size, void const* prefix_address = nullptr,
                         std::size_t alignment = 16) -> void*;

    // Only allocation-start pointers returned by this instance may be passed.
    // Pool frees require the original byte size; mismatches are ignored.
    auto deallocate(void* address, std::size_t size) -> void;

    auto get_shared_memory_name() const -> std::string;

    auto get_memory_size_bytes() const -> std::size_t;

    auto get_shared_memory_start_address() const -> void*;

private:
    friend class util::Singleton<BasicMemory>;
    BasicMemory();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace kitzoo::memory

#endif  // KITZOO_MEMORY_BASIC_MEMORY_HPP
