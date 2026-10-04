// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/concurrent_object_pool.hpp
// Description: Declares a concurrent object pool backed by moodycamel
//              ConcurrentQueue for non-blocking lease acquisition.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_CONCURRENT_OBJECT_POOL_HPP
#define KITZOO_THREAD_CONCURRENT_OBJECT_POOL_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/queue/concurrent_queue.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace kitzoo::thread {

template <typename T>
class ConcurrentObjectPool {
public:
    struct Deleter {
        ConcurrentObjectPool* pool;

        auto operator()(T* object) const noexcept -> void {
            if (object != nullptr)
                (void)pool->objects_.enqueue(std::unique_ptr<T>{object});
        }
    };

    using UniquePtr = std::unique_ptr<T, Deleter>;
    using SharedPtr = std::shared_ptr<T>;

    ConcurrentObjectPool() = default;
    ConcurrentObjectPool(ConcurrentObjectPool const&) = delete;
    auto operator=(ConcurrentObjectPool const&) -> ConcurrentObjectPool& = delete;
    ConcurrentObjectPool(ConcurrentObjectPool&&) = delete;
    auto operator=(ConcurrentObjectPool&&) -> ConcurrentObjectPool& = delete;

    auto add(std::unique_ptr<T> object) -> void {
        if (!object)
            throw std::invalid_argument{"cannot add a null object"};
        if (!objects_.enqueue(std::move(object)))
            throw std::bad_alloc{};
    }

    KZ_NODISCARD auto acquire() -> UniquePtr {
        std::unique_ptr<T> object;
        objects_.wait_dequeue(object);
        return UniquePtr{object.release(), Deleter{this}};
    }

    KZ_NODISCARD auto try_acquire() -> std::optional<UniquePtr> {
        std::unique_ptr<T> object;
        if (!objects_.try_dequeue(object))
            return std::nullopt;
        return UniquePtr{object.release(), Deleter{this}};
    }

    template <typename Rep, typename Period>
    KZ_NODISCARD auto acquire_for(std::chrono::duration<Rep, Period> timeout)
        -> std::optional<UniquePtr> {
        std::unique_ptr<T> object;
        if (!objects_.wait_dequeue_timed(object, timeout))
            return std::nullopt;
        return UniquePtr{object.release(), Deleter{this}};
    }

    KZ_NODISCARD auto acquire_shared() -> SharedPtr { return SharedPtr{acquire()}; }

    KZ_NODISCARD auto available_approx() const noexcept -> std::size_t {
        return objects_.size_approx();
    }

private:
    kitzoo::queue::BlockingConcurrentQueue<std::unique_ptr<T>> objects_;
};

}  // namespace kitzoo::thread

#endif  // KITZOO_THREAD_CONCURRENT_OBJECT_POOL_HPP
