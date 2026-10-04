// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/byte_buffer.hpp
// Description: Declares movable mimalloc byte storage with optional alignment and growth.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_BYTE_BUFFER_HPP
#define KITZOO_MEMORY_BYTE_BUFFER_HPP

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

    ByteBuffer(ByteBuffer const&) = delete;
    auto operator=(ByteBuffer const&) -> ByteBuffer& = delete;

    ByteBuffer(ByteBuffer&& other) noexcept;
    auto operator=(ByteBuffer&& other) noexcept -> ByteBuffer&;

    auto data() noexcept -> std::byte*;
    auto data() const noexcept -> std::byte const*;

    // Non-owning views cover size(), not capacity(), and follow data()'s lifetime.
    auto view() noexcept -> std::span<std::byte>;
    auto view() const noexcept -> std::span<std::byte const>;

    auto size() const noexcept -> std::size_t;

    auto capacity() const noexcept -> std::size_t;

    auto alignment() const noexcept -> std::size_t;

    auto empty() const noexcept -> bool;

    static constexpr auto max_size() noexcept -> std::size_t {
        return static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    }

    // reserve() leaves size unchanged. resize() grows capacity geometrically.
    auto reserve(std::size_t capacity) -> void;

    auto resize(std::size_t size) -> void;

    // Self/subrange append is supported. Internal sources must lie within size().
    // Empty input is a no-op; nonempty input must point to readable bytes.
    auto append(std::span<std::byte const> bytes) -> void;

    auto append(void const* data, std::size_t size) -> void;

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

}  // namespace kitzoo::memory

#endif  // KITZOO_MEMORY_BYTE_BUFFER_HPP
