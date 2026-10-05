// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/memory/bytebuffer.cpp
// Description: Implements aligned byte-buffer growth and exception-safe mimalloc ownership.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/bytebuffer.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mimalloc.h>
#include <new>
#include <stdexcept>
#include <utility>

namespace kitzoo::memory {

ByteBuffer::ByteBuffer(std::size_t size, std::size_t alignment) : alignment_(alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    throw std::invalid_argument("ByteBuffer alignment must be a nonzero power of two");
  resize(size);
}

ByteBuffer::~ByteBuffer() {
  mi_free(data_);
}

ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)), size_(std::exchange(other.size_, 0)),
      capacity_(std::exchange(other.capacity_, 0)), alignment_(other.alignment_) {}

auto ByteBuffer::operator=(ByteBuffer&& other) noexcept -> ByteBuffer& {
  if (this != &other) {
    reset();
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    capacity_ = std::exchange(other.capacity_, 0);
    alignment_ = other.alignment_;
  }
  return *this;
}

auto ByteBuffer::data() noexcept -> std::byte* {
  return data_;
}

auto ByteBuffer::data() const noexcept -> std::byte const* {
  return data_;
}

auto ByteBuffer::view() noexcept -> std::span<std::byte> {
  return {data_, size_};
}

auto ByteBuffer::view() const noexcept -> std::span<std::byte const> {
  return {data_, size_};
}

auto ByteBuffer::size() const noexcept -> std::size_t {
  return size_;
}

auto ByteBuffer::capacity() const noexcept -> std::size_t {
  return capacity_;
}

auto ByteBuffer::alignment() const noexcept -> std::size_t {
  return alignment_;
}

auto ByteBuffer::empty() const noexcept -> bool {
  return size_ == 0;
}

auto ByteBuffer::reallocate(std::size_t capacity) -> void {
  if (capacity > max_size())
    throw std::length_error("ByteBuffer capacity exceeds max_size");
  auto* address = alignment_ > alignof(std::max_align_t) ? mi_realloc_aligned(data_, capacity, alignment_)
                                                         : mi_realloc(data_, capacity);
  if (!address)
    throw std::bad_alloc{};
  // realloc failure leaves the original allocation live. Commit only on success.
  data_ = static_cast<std::byte*>(address);
  capacity_ = capacity;
}

auto ByteBuffer::reserve(std::size_t capacity) -> void {
  if (capacity > capacity_)
    reallocate(capacity);
}

auto ByteBuffer::resize(std::size_t size) -> void {
  if (size > max_size())
    throw std::length_error("ByteBuffer size exceeds max_size");
  if (size > capacity_) {
    auto const growth = capacity_ > max_size() - capacity_ / 2 ? max_size() : capacity_ + capacity_ / 2;
    reserve(std::max(size, growth));
  }
  size_ = size;
}

auto ByteBuffer::clear() noexcept -> void {
  size_ = 0;
}

auto ByteBuffer::append(std::span<std::byte const> bytes) -> void {
  append(bytes.data(), bytes.size());
}

auto ByteBuffer::append(void const* data, std::size_t size) -> void {
  if (size == 0)
    return;
  if (size > max_size() - size_)
    throw std::length_error("ByteBuffer append exceeds max_size");
  if (!data)
    throw std::invalid_argument("ByteBuffer append requires non-null input");

  auto const source_address = reinterpret_cast<std::uintptr_t>(data);
  auto const base_address = reinterpret_cast<std::uintptr_t>(data_);
  auto const internal = data_ && source_address >= base_address && source_address - base_address < capacity_;
  std::size_t offset = 0;
  if (internal) {
    offset = source_address - base_address;
    if (offset > size_ || size > size_ - offset)
      throw std::invalid_argument("ByteBuffer append source exceeds its logical size");
  }

  auto const previous_size = size_;
  resize(previous_size + size);
  // Rebuild aliased sources after growth, which may have moved the allocation.
  auto const* source = internal ? data_ + offset : static_cast<std::byte const*>(data);
  std::memmove(data_ + previous_size, source, size);
}

auto ByteBuffer::reset() noexcept -> void {
  mi_free(data_);
  data_ = nullptr;
  size_ = 0;
  capacity_ = 0;
}

auto ByteBuffer::shrink_to_fit() -> void {
  if (size_ == 0)
    reset();
  else if (size_ < capacity_)
    reallocate(size_);
}

auto ByteBuffer::swap(ByteBuffer& other) noexcept -> void {
  std::swap(data_, other.data_);
  std::swap(size_, other.size_);
  std::swap(capacity_, other.capacity_);
  std::swap(alignment_, other.alignment_);
}

auto swap(ByteBuffer& left, ByteBuffer& right) noexcept -> void {
  left.swap(right);
}

} // namespace kitzoo::memory
