// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/basicmemory_test.cpp
// Description: Verifies singleton budgets, shared mappings, fallback and concurrent allocation.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/basicmemory.hpp>
#include <kitzoo/memory/memory.hpp>

#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <thread>
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

namespace {
constexpr std::size_t kVirtualBytes = 65536;
} // namespace

// Initialization is one-shot, so these singleton scenarios run in one test.
TEST(BasicMemoryTest, PoolsMappingsAndConcurrentAllocation) {
  using namespace kitzoo::memory;
  auto& memory = BasicMemory::instance();
  EXPECT_EQ(&memory, &BasicMemory::instance());
  auto* fallback = memory.allocate(8);
  ASSERT_NE(fallback, nullptr);
  EXPECT_EQ(memory.memory_type(fallback), MemoryType::Unknown);
  EXPECT_FALSE(memory.init_virtual({0}));
  ASSERT_TRUE(memory.init_virtual({kVirtualBytes}));
  EXPECT_FALSE(memory.init_virtual({kVirtualBytes}));
  memory.deallocate(fallback, 8); // A direct allocation remains valid after initialization.
  EXPECT_EQ(memory.allocate(0), nullptr);
  EXPECT_EQ(memory.allocate(16, nullptr, 3), nullptr);
  EXPECT_EQ(memory.allocate(std::numeric_limits<std::size_t>::max()), nullptr);

  auto* full = memory.allocate(kVirtualBytes);
  ASSERT_NE(full, nullptr);
  EXPECT_EQ(memory.memory_type(full), MemoryType::Virtual);
  EXPECT_EQ(memory.allocate(1), nullptr);
  memory.deallocate(full, kVirtualBytes - 1);
  EXPECT_EQ(memory.allocate(1), nullptr); // Wrong size must not release the allocation.
  memory.deallocate(full, kVirtualBytes);

  std::vector<void*> blocks;
  for (int i = 0; i < 4; ++i)
    blocks.push_back(memory.allocate(kVirtualBytes / 4));
  for (auto* block : blocks)
    ASSERT_NE(block, nullptr);
  memory.deallocate(blocks[0], kVirtualBytes / 4);
  memory.deallocate(blocks[2], kVirtualBytes / 4);
  // Freed bytes are reusable without the old contiguous-pool fragmentation.
  auto* reused = memory.allocate(kVirtualBytes / 2);
  ASSERT_NE(reused, nullptr);
  memory.deallocate(reused, kVirtualBytes / 2);
  memory.deallocate(blocks[1], kVirtualBytes / 4);
  memory.deallocate(blocks[3], kVirtualBytes / 4);

  auto* aligned = memory.allocate(64, nullptr, 64);
  ASSERT_NE(aligned, nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % 64, 0U);
  memory.deallocate(aligned, 64);
  memory.deallocate(nullptr, 0);

#if defined(_WIN32)
  auto name = "Local\\kitzoo_memory_" + std::to_string(GetCurrentProcessId());
#else
  auto name = "/kitzoo_memory_" + std::to_string(getpid());
#endif
  EXPECT_FALSE(memory.init_shared({"", 128}));
#if defined(_WIN32)
  auto existing = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 128, name.c_str());
  ASSERT_NE(existing, nullptr);
  EXPECT_FALSE(memory.init_shared({name, 128}));
  CloseHandle(existing);
#else
  auto existing = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
  ASSERT_GE(existing, 0);
  ASSERT_EQ(ftruncate(existing, 128), 0);
  EXPECT_FALSE(memory.init_shared({name, 128}));
  auto still_exists = shm_open(name.c_str(), O_RDONLY, 0);
  EXPECT_GE(still_exists, 0);
  if (still_exists >= 0)
    close(still_exists);
  close(existing);
  shm_unlink(name.c_str());
#endif
  ASSERT_TRUE(memory.init_shared({name, 128}));
  EXPECT_FALSE(memory.init_shared({name, 128}));
  EXPECT_EQ(memory.shared_memory_name(), name);
  EXPECT_EQ(memory.shared_memory_size_bytes(), 128U);
  auto* shared = memory.allocate_shared(128);
  ASSERT_NE(shared, nullptr);
  EXPECT_EQ(shared, memory.shared_memory_address());
  EXPECT_EQ(memory.memory_type(shared), MemoryType::Shared);
  std::memset(shared, 0x5a, 128);
#if defined(_WIN32)
  auto handle = OpenFileMappingA(FILE_MAP_READ, FALSE, name.c_str());
  ASSERT_NE(handle, nullptr);
  auto* view = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, 128);
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(static_cast<unsigned char*>(view)[127], 0x5a);
  UnmapViewOfFile(view);
  CloseHandle(handle);
#else
  auto fd = shm_open(name.c_str(), O_RDONLY, 0);
  ASSERT_GE(fd, 0);
  auto* view = mmap(nullptr, 128, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  ASSERT_NE(view, MAP_FAILED);
  EXPECT_EQ(static_cast<unsigned char*>(view)[127], 0x5a);
  munmap(view, 128);
#endif
  auto* spill = memory.allocate_shared(144);
  ASSERT_NE(spill, nullptr);
  EXPECT_EQ(memory.memory_type(spill), MemoryType::Virtual);
  memory.deallocate(spill, 144);
  memory.deallocate(shared, 128);

  // Freed neighbours coalesce, so a fragmented pool becomes contiguous again.
  void* quarters[4];
  for (auto*& quarter : quarters) {
    quarter = memory.allocate_shared(32);
    ASSERT_EQ(memory.memory_type(quarter), MemoryType::Shared);
  }
  memory.deallocate(quarters[0], 32);
  memory.deallocate(quarters[2], 32);
  auto* fragmented = memory.allocate_shared(64);
  EXPECT_EQ(memory.memory_type(fragmented), MemoryType::Virtual);
  memory.deallocate(fragmented, 64);
  memory.deallocate(quarters[1], 31); // A wrong size must not release the block.
  auto* still_fragmented = memory.allocate_shared(64);
  EXPECT_EQ(memory.memory_type(still_fragmented), MemoryType::Virtual);
  memory.deallocate(still_fragmented, 64);
  memory.deallocate(quarters[1], 32);
  auto* merged = memory.allocate_shared(96);
  EXPECT_EQ(merged, memory.shared_memory_address());
  memory.deallocate(merged, 96);
  memory.deallocate(quarters[3], 32);

  {
    std::pmr::vector<int> mapped{memory.shared_resource()};
    mapped.push_back(42);
    EXPECT_EQ(memory.memory_type(mapped.data()), MemoryType::Shared);
    EXPECT_EQ(mapped[0], 42);
  }

  std::atomic<bool> failed = false;
  std::vector<std::thread> workers;
  for (int i = 0; i < 8; ++i) {
    workers.emplace_back([&memory, &failed]() -> void {
      for (int iteration = 0; iteration < 1000; ++iteration) {
        auto* address = memory.allocate(17);
        if (!address) {
          failed = true;
          break;
        }
        std::memset(address, 0xa5, 17);
        memory.deallocate(address, 17);
      }
    });
  }
  for (auto& worker : workers)
    worker.join();
  EXPECT_FALSE(failed);
  full = memory.allocate(kVirtualBytes);
  ASSERT_NE(full, nullptr);
  memory.deallocate(full, kVirtualBytes);

  // Leave enough room for array data, but none for the shared_ptr control block.
  auto* reserved = memory.allocate(kVirtualBytes - sizeof(int));
  ASSERT_NE(reserved, nullptr);
  Memory objects(memory.resource());
  EXPECT_THROW(objects.alloc_basic_array<int>(1), std::bad_alloc);
  memory.deallocate(reserved, kVirtualBytes - sizeof(int));
  full = memory.allocate(kVirtualBytes);
  ASSERT_NE(full, nullptr); // Failed control-block allocation reclaimed array data.
  memory.deallocate(full, kVirtualBytes);
}
