// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/memory.cpp
// Description: Demonstrates direct mimalloc allocation, optional budgets and shared storage.
// -----------------------------------------------------------------------------

#include <kitzoo/memory.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

namespace memory = kitzoo::memory;

constexpr std::size_t kBudgetBytes = 64 * 1024;
constexpr std::size_t kSharedBytes = 4096;

// A cross-process payload contains values, not pointers or owning STL objects.
struct SharedRecord {
    std::uint32_t frame_id = 7;
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    std::uint32_t channels = 3;
};

static_assert(std::is_trivially_copyable_v<SharedRecord>);

auto demonstrate_containers() -> void {
    std::printf("[1] STL containers with explicit mimalloc allocation\n");

    // Explicit standard containers and the convenient aliases use the same
    // MiAllocator. Ordinary std::vector<int> still uses its standard allocator.
    std::vector<int, memory::MiAllocator<int>> values{1, 2, 3};
    memory::Vector<memory::String> names;
    names.emplace_back("front camera");
    memory::Map<int, memory::String> labels;
    labels.try_emplace(1, "capture");
    std::printf("values: %zu, name: %s, label: %s\n", values.size(), names[0].c_str(),
                labels.at(1).c_str());
}

auto demonstrate_smart_pointers() -> void {
    std::printf("\n[2] Shared, weak and unique ownership\n");

    // Object storage and shared_ptr control blocks use mimalloc too. Use our
    // String alias when its character storage should follow the same resource.
    auto object = memory::make_shared<memory::String>("mimalloc-backed object");
    memory::WeakPtr<memory::String> weak = object;
    {
        auto locked = weak.lock();
        std::printf("weak lock shares the object: %s\n",
                    locked.get() == object.get() ? "yes" : "no");
    }
    memory::Memory objects;
    auto array = objects.alloc_basic_array<int>(4);  // Value-initialized to zero.
    array[3] = 42;
    std::printf("object: %s, array[0]: %d, array[3]: %d\n", object->c_str(), array[0], array[3]);
    object.reset();
    std::printf("weak reference expired after object release: %s\n", weak.expired() ? "yes" : "no");
    weak.reset();

    // make_unique returns UniquePtr with a stateless mimalloc deleter. Both
    // object destruction and storage release happen automatically at scope exit.
    memory::UniquePtr<memory::String> unique =
        memory::make_unique<memory::String>("exclusive mimalloc object");
    auto moved = std::move(unique);
    std::printf("unique object: %s, source empty after move: %s\n", moved->c_str(),
                unique ? "no" : "yes");
    moved.reset();  // Destroy the object and return storage to mimalloc.
}

auto demonstrate_byte_buffer() -> void {
    std::printf("\n[3] Aligned byte buffer and capacity reuse\n");

    // Raw buffers are useful for image/file/network data. New bytes are not
    // initialized; initialize the portion written before exposing it to readers.
    memory::ByteBuffer buffer{256, 64};
    std::memset(buffer.data(), 0x5a, buffer.size());
    buffer.reserve(1024);
    buffer.resize(512);  // Preserves the original 256 bytes; new bytes are uninitialized.
    std::memset(buffer.data() + 256, 0, 256);
    std::printf("buffer: %zu bytes, capacity: %zu, alignment: %zu, first byte: %u\n", buffer.size(),
                buffer.capacity(), buffer.alignment(), std::to_integer<unsigned>(buffer.data()[0]));
    std::byte const trailer[]{std::byte{0x01}, std::byte{0x02}};
    buffer.append(std::span{trailer});
    buffer.append(buffer.view().first(4));  // Self-append remains safe across growth.
    auto const& read_only = buffer;
    std::printf("after append: %zu bytes, read-only view: %zu bytes\n", buffer.size(),
                read_only.view().size());
    // Views do not own storage; reacquire them after buffer growth or reset.
    auto owned = std::move(buffer);  // Transfers ownership without copying bytes.
    owned.clear();                   // Retain allocation for the next operation.
    owned.reset();                   // Return storage to mimalloc immediately.
}

auto demonstrate_bounded_allocation() -> void {
    std::printf("\n[4] PMR resource with a %zu-byte budget\n", kBudgetBytes);

    // This resource counts live requested bytes, not RSS, mimalloc overhead or
    // bookkeeping. Declare it before its users so that it is destroyed last.
    memory::LimitedResource budget{kBudgetBytes};
    memory::WeakPtr<std::pmr::string> weak;
    {
        std::pmr::vector<std::pmr::string> names{&budget};
        names.emplace_back(128, 'x');  // Nested String inherits the selected resource.
        memory::Memory objects{&budget};
        auto object = memory::make_shared_with_resource<std::pmr::string>(&budget, 128, 'y');
        weak = object;
        auto array = objects.alloc_basic_array<int>(16);
        array[0] = 42;
        auto stats = budget.stats();
        std::printf("nested String uses budget: %s, array[0]: %d\n",
                    names[0].get_allocator().resource() == &budget ? "yes" : "no", array[0]);
        std::printf("live: %zu bytes, peak: %zu bytes, allocations: %zu\n", stats.used_bytes,
                    stats.peak_bytes, stats.allocation_count);

        bool rejected = false;
        try {
            std::pmr::vector<std::byte> oversized{&budget};
            oversized.resize(kBudgetBytes + 1);
        } catch (std::bad_alloc const&) {
            rejected = true;
            std::printf("request exceeding the budget: rejected with std::bad_alloc\n");
        }
        if (!rejected)
            throw std::runtime_error("The budget failed to reject an oversized request");
    }

    // The object is gone, but the weak reference retains its control block.
    std::printf("after object destruction: weak expired: %s, %zu bytes still charged\n",
                weak.expired() ? "yes" : "no", budget.stats().used_bytes);
    weak.reset();  // Release the control block before destroying its resource.
    auto const stats = budget.stats();
    std::printf("after destruction: %zu live bytes, %zu live allocations\n", stats.used_bytes,
                stats.allocation_count);
    if (stats.used_bytes != 0 || stats.allocation_count != 0)
        throw std::runtime_error("Budget allocations were not fully released");
}

auto demonstrate_shared_storage() -> void {
    std::printf("\n[5] Native shared mapping for a value-only payload\n");

    auto& basic = memory::BasicMemory::instance();
    auto const suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#if defined(_WIN32)
    auto const name = "Local\\kitzoo_memory_example_" + suffix;
#else
    auto const name = "/kitzoo_memory_example_" + suffix;
#endif
    if (!basic.init_shared({name, kSharedBytes}))
        throw std::runtime_error("Could not create the shared mapping");

    // Explicit PMR allocation selects shared storage through a resource. The mapping
    // is owned by BasicMemory and released at shutdown; on POSIX its name is unlinked.
    auto allocator = std::pmr::polymorphic_allocator<SharedRecord>{basic.shared_resource()};
    auto* record = allocator.new_object<SharedRecord>();
    if (basic.memory_type(record) != memory::MemoryType::Shared) {
        allocator.delete_object(record);
        throw std::runtime_error("The record unexpectedly used fallback storage");
    }
    auto const offset = reinterpret_cast<std::uintptr_t>(record) -
                        reinterpret_cast<std::uintptr_t>(basic.get_shared_memory_start_address());
    std::printf("mapping: %s, payload offset: %zu bytes\n", name.c_str(),
                static_cast<std::size_t>(offset));
    std::printf("frame %u: %u x %u, %u channels\n", record->frame_id, record->width, record->height,
                record->channels);

    // A reader maps the same name and locates the payload by offset. Coordinate
    // publication and lifetime separately. Only the creator allocates/frees;
    // raw pointers, containers and shared_ptr control blocks are process-local.
    allocator.delete_object(record);

    // Shared exhaustion may fall back to BasicMemory's ordinary allocation path.
    // No virtual budget is configured here, so the fallback is direct mimalloc.
    std::pmr::vector<std::byte> fallback{basic.shared_resource()};
    fallback.resize(kSharedBytes + 1);
    std::printf("request larger than mapping used direct fallback: %s\n",
                basic.memory_type(fallback.data()) == memory::MemoryType::Unknown ? "yes" : "no");
}

}  // namespace

auto main() -> int {
    try {
        demonstrate_containers();
        demonstrate_smart_pointers();
        demonstrate_byte_buffer();
        demonstrate_bounded_allocation();
        demonstrate_shared_storage();
        return 0;
    } catch (std::exception const& error) {
        std::fprintf(stderr, "Memory example failed: %s\n", error.what());
        return 1;
    }
}
