// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/bytebuffer_test.cpp
// Description: Verifies aligned byte storage, growth, ownership transfer and aliased appends.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/bytebuffer.hpp>

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <type_traits>

TEST(ByteBufferTest, GrowthPreservesDataAndClearRetainsCapacity) {
  using kitzoo::memory::ByteBuffer;
  ByteBuffer buffer;
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.data(), nullptr);
  buffer.resize(16);
  std::memset(buffer.data(), 0x5a, buffer.size());
  buffer.reserve(128);
  EXPECT_EQ(buffer.size(), 16U);
  EXPECT_EQ(buffer.capacity(), 128U);
  buffer.resize(256);
  for (std::size_t i = 0; i < 16; ++i)
    EXPECT_EQ(buffer.data()[i], std::byte{0x5a});
  auto* address = buffer.data();
  const auto capacity = buffer.capacity();
  buffer.resize(8);
  EXPECT_EQ(buffer.data(), address);
  EXPECT_EQ(buffer.capacity(), capacity);
  buffer.clear();
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.data(), address);
  buffer.resize(8);
  buffer.shrink_to_fit();
  EXPECT_EQ(buffer.size(), 8U);
  EXPECT_EQ(buffer.capacity(), 8U);
  EXPECT_EQ(buffer.data()[7], std::byte{0x5a});
  buffer.clear();
  buffer.shrink_to_fit();
  EXPECT_EQ(buffer.data(), nullptr);
  EXPECT_EQ(buffer.capacity(), 0U);
  buffer.reserve(8);
  EXPECT_EQ(buffer.size(), 0U);
  buffer.reset();
  EXPECT_EQ(buffer.capacity(), 0U);
}

TEST(ByteBufferTest, AlignmentMoveAssignmentSwapAndReuse) {
  using kitzoo::memory::ByteBuffer;
  static_assert(!std::is_copy_constructible_v<ByteBuffer>);
  static_assert(std::is_nothrow_move_constructible_v<ByteBuffer>);
  ByteBuffer buffer{16, 256};
  buffer.data()[0] = std::byte{0x42};
  buffer.resize(8192);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(buffer.data()) % 256, 0U);
  EXPECT_EQ(buffer.data()[0], std::byte{0x42});
  auto* address = buffer.data();
  ByteBuffer moved{std::move(buffer)};
  EXPECT_EQ(moved.data(), address);
  EXPECT_EQ(buffer.data(), nullptr);
  EXPECT_EQ(buffer.capacity(), 0U);
  EXPECT_EQ(buffer.size(), 0U);
  buffer.resize(8); // Moved-from objects remain reusable with their alignment.
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(buffer.data()) % 256, 0U);
  ByteBuffer destination{32};
  destination = std::move(moved);
  EXPECT_EQ(destination.data(), address);
  EXPECT_EQ(destination.alignment(), 256U);
  EXPECT_TRUE(moved.empty());
  destination.resize(8);
  destination.shrink_to_fit();
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(destination.data()) % 256, 0U);
  EXPECT_EQ(destination.data()[0], std::byte{0x42});
  swap(destination, buffer);
  EXPECT_EQ(buffer.data()[0], std::byte{0x42});
  EXPECT_EQ(buffer.alignment(), 256U);
  ByteBuffer ordinary{8};
  swap(buffer, ordinary);
  EXPECT_EQ(ordinary.alignment(), 256U);
  EXPECT_EQ(ordinary.data()[0], std::byte{0x42});
}

TEST(ByteBufferTest, InvalidRequestsAndFailedGrowthPreserveState) {
  using kitzoo::memory::ByteBuffer;
  EXPECT_THROW(ByteBuffer(0, 0), std::invalid_argument);
  EXPECT_THROW(ByteBuffer(0, 3), std::invalid_argument);
  for (auto alignment : {alignof(std::max_align_t), std::size_t{256}}) {
    ByteBuffer buffer{8, alignment};
    buffer.data()[0] = std::byte{0x42};
    auto* address = buffer.data();
    const auto capacity = buffer.capacity();
    EXPECT_THROW(buffer.resize(std::numeric_limits<std::size_t>::max()), std::length_error);
    // A near-PTRDIFF_MAX allocation exceeds supported OS address spaces.
    EXPECT_THROW(buffer.reserve(ByteBuffer::max_size()), std::bad_alloc);
    EXPECT_EQ(buffer.data(), address);
    EXPECT_EQ(buffer.size(), 8U);
    EXPECT_EQ(buffer.capacity(), capacity);
    EXPECT_EQ(buffer.data()[0], std::byte{0x42});
  }
}

TEST(ByteBufferTest, AppendExternalBytesSelfAndSubranges) {
  using kitzoo::memory::ByteBuffer;
  ByteBuffer buffer{0, 64};
  const std::byte input[]{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  buffer.append(std::span{input});
  EXPECT_EQ(buffer.size(), 4U);
  buffer.append(buffer.view()); // Forces growth beyond the original capacity.
  ASSERT_EQ(buffer.size(), 8U);
  for (std::size_t i = 0; i < 8; ++i)
    EXPECT_EQ(buffer.view()[i], input[i % 4]);
  buffer.reserve(64);
  auto* address = buffer.data();
  buffer.append(buffer.view().subspan(1, 2)); // Reuses the allocation.
  EXPECT_EQ(buffer.data(), address);
  ASSERT_EQ(buffer.size(), 10U);
  EXPECT_EQ(buffer.view()[8], std::byte{2});
  EXPECT_EQ(buffer.view()[9], std::byte{3});
  buffer.append(buffer.data() + 8, 2);
  EXPECT_EQ(buffer.size(), 12U);
  EXPECT_EQ(buffer.view()[10], std::byte{2});
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(buffer.data()) % 64, 0U);
  buffer.view()[0] = std::byte{9};
  const auto& read_only = buffer;
  static_assert(std::is_same_v<decltype(read_only.view()), std::span<const std::byte>>);
  EXPECT_EQ(read_only.view().size(), buffer.size());
  EXPECT_EQ(read_only.view()[0], std::byte{9});
  ByteBuffer empty;
  EXPECT_TRUE(empty.view().empty());
  empty.append(empty.view());
  empty.append(nullptr, 0);
  EXPECT_EQ(empty.data(), nullptr);
}

TEST(ByteBufferTest, InvalidAppendPreservesContentsAndAllocation) {
  using kitzoo::memory::ByteBuffer;
  ByteBuffer buffer{4};
  std::memset(buffer.data(), 0x5a, buffer.size());
  buffer.reserve(16);
  auto* address = buffer.data();
  const auto capacity = buffer.capacity();
  EXPECT_THROW(buffer.append(nullptr, 1), std::invalid_argument);
  EXPECT_THROW(buffer.append(buffer.data() + 3, 2), std::invalid_argument);
  EXPECT_THROW(buffer.append(buffer.data() + 8, 1), std::invalid_argument);
  EXPECT_THROW(buffer.append(buffer.data(), ByteBuffer::max_size()), std::length_error);
  EXPECT_EQ(buffer.data(), address);
  EXPECT_EQ(buffer.size(), 4U);
  EXPECT_EQ(buffer.capacity(), capacity);
  for (auto byte : buffer.view())
    EXPECT_EQ(byte, std::byte{0x5a});
}
