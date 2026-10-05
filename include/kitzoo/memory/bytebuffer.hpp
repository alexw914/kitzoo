// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/bytebuffer.hpp
// Description: Declares movable mimalloc byte storage with optional alignment and growth.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_BYTEBUFFER_HPP
#define KITZOO_MEMORY_BYTEBUFFER_HPP

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <limits>
#include <span>

namespace kitzoo::memory {

// Raw bytes are uninitialized, including newly exposed bytes after resize().
// Growth may invalidate pointers; failed allocation preserves pointer and contents.
class ByteBuffer {
public:
  explicit ByteBuffer(std::size_t size = 0, std::size_t alignment = alignof(std::max_align_t));

  ~ByteBuffer();

  ByteBuffer(const ByteBuffer&) = delete;
  auto operator=(const ByteBuffer&) -> ByteBuffer& = delete;

  ByteBuffer(ByteBuffer&& other) noexcept;
  auto operator=(ByteBuffer&& other) noexcept -> ByteBuffer&;

  auto data() noexcept -> std::byte*;
  KZ_NODISCARD auto data() const noexcept -> const std::byte*;

  // Non-owning views cover size(), not capacity(), and follow data()'s lifetime.
  auto view() noexcept -> std::span<std::byte>;
  KZ_NODISCARD auto view() const noexcept -> std::span<const std::byte>;

  KZ_NODISCARD auto size() const noexcept -> std::size_t;

  KZ_NODISCARD auto capacity() const noexcept -> std::size_t;

  KZ_NODISCARD auto alignment() const noexcept -> std::size_t;

  KZ_NODISCARD auto empty() const noexcept -> bool;

  static constexpr auto max_size() noexcept -> std::size_t {
    return static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
  }

  // reserve() leaves size unchanged. resize() grows capacity geometrically.
  auto reserve(std::size_t capacity) -> void;

  auto resize(std::size_t size) -> void;

  // Self/subrange append is supported. Internal sources must lie within size().
  // Empty input is a no-op; nonempty input must point to readable bytes.
  auto append(std::span<const std::byte> bytes) -> void;

  auto append(const void* data, std::size_t size) -> void;

  // Retains capacity for reuse. reset() releases storage instead.
  auto clear() noexcept -> void;

  auto reset() noexcept -> void;

  auto shrink_to_fit() -> void;

  auto swap(ByteBuffer& other) noexcept -> void;

private:
  auto reallocate(std::size_t capacity) -> void;

  std::byte* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
  std::size_t alignment_;
};

auto swap(ByteBuffer& left, ByteBuffer& right) noexcept -> void;

} // namespace kitzoo::memory

#endif // KITZOO_MEMORY_BYTEBUFFER_HPP
