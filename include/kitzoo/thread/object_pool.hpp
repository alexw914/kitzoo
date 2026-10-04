// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/object_pool.hpp
// Description: Declares a non-thread-safe reusable object pool with leases that
//              return objects to the pool on release.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_OBJECT_POOL_HPP
#define KITZOO_THREAD_OBJECT_POOL_HPP

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace kitzoo::thread {

template <typename T>
class ObjectPool {
    union Slot {
        Slot* next;
        alignas(T) std::byte storage[sizeof(T)];
    };

public:
    struct Deleter {
        ObjectPool* pool;

        auto operator()(T* ptr) const noexcept -> void { pool->destroy(ptr); }
    };

    using UniquePtr = std::unique_ptr<T, Deleter>;
    using SharedPtr = std::shared_ptr<T>;

    explicit ObjectPool(std::size_t chunk_size = 64)
        : chunk_size_{chunk_size == 0 ? 64 : chunk_size} {}

    ObjectPool(ObjectPool const&) = delete;
    auto operator=(ObjectPool const&) -> ObjectPool& = delete;
    auto operator=(ObjectPool&&) -> ObjectPool& = delete;
    ObjectPool(ObjectPool&&) = delete;

    template <typename... Args>
    KZ_NODISCARD auto acquire(Args&&... args) -> UniquePtr {
        return UniquePtr{construct(std::forward<Args>(args)...), Deleter{this}};
    }

    template <typename... Args>
    KZ_NODISCARD auto acquire_shared(Args&&... args) -> SharedPtr {
        auto object = acquire(std::forward<Args>(args)...);
        return SharedPtr{object.release(), [this](T* ptr) { destroy(ptr); }};
    }

    template <typename... Args>
    KZ_NODISCARD auto construct(Args&&... args) -> T* {
        if (free_ == nullptr)
            grow();
        Slot* slot = free_;
        free_ = slot->next;
        auto* object = std::launder(reinterpret_cast<T*>(slot->storage));
        try {
            std::construct_at(object, std::forward<Args>(args)...);
        } catch (...) {
            slot->next = free_;
            free_ = slot;
            throw;
        }
        ++live_;
        return object;
    }

    auto destroy(T* object) noexcept -> void {
        if (object == nullptr)
            return;
        std::destroy_at(object);
        auto* slot = reinterpret_cast<Slot*>(object);
        slot->next = free_;
        free_ = slot;
        --live_;
    }

    KZ_NODISCARD auto allocated_count() const noexcept -> std::size_t { return live_; }

private:
    auto grow() -> void {
        auto chunk = std::make_unique<Slot[]>(chunk_size_);
        auto* slots = chunk.get();
        for (std::size_t i = 0; i + 1 < chunk_size_; ++i)
            slots[i].next = &slots[i + 1];
        slots[chunk_size_ - 1].next = nullptr;
        free_ = slots;
        chunks_.push_back(std::move(chunk));
    }

    std::size_t chunk_size_;
    std::vector<std::unique_ptr<Slot[]>> chunks_;
    Slot* free_{nullptr};
    std::size_t live_{0};
};

}  // namespace kitzoo::thread

#endif  // KITZOO_THREAD_OBJECT_POOL_HPP
