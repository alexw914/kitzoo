// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/blocking_object_pool.hpp
// Description: Declares a thread-safe object pool that blocks when empty and
//              coordinates lease return, close, and waiting.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kitzoo::thread {

template <typename T>
class BlockingObjectPool {
    struct Entry {
        explicit Entry(std::unique_ptr<T> value) : object{std::move(value)} {}
        std::unique_ptr<T> object;
        Entry* next{};
    };

public:
    struct Deleter {
        BlockingObjectPool* pool{};
        Entry* entry{};
        auto operator()(T*) const noexcept -> void {
            if (pool != nullptr)
                pool->release(entry);
        }
    };
    using UniquePtr = std::unique_ptr<T, Deleter>;
    using SharedPtr = std::shared_ptr<T>;

    BlockingObjectPool() = default;
    BlockingObjectPool(BlockingObjectPool const&) = delete;
    auto operator=(BlockingObjectPool const&) -> BlockingObjectPool& = delete;
    BlockingObjectPool(BlockingObjectPool&&) = delete;
    auto operator=(BlockingObjectPool&&) -> BlockingObjectPool& = delete;

    auto add(std::unique_ptr<T> object) -> void {
        if (!object)
            throw std::invalid_argument{"cannot add a null object"};

        auto entry = std::make_unique<Entry>(std::move(object));
        auto* raw = entry.get();
        {
            std::lock_guard lock{mutex_};
            if (closed_)
                throw std::logic_error{"cannot add to a closed object pool"};
            entries_.push_back(std::move(entry));
            raw->next = free_;
            free_ = raw;
            ++available_;
        }
        cv_.notify_one();
    }

    KZ_NODISCARD auto acquire() -> UniquePtr {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return closed_ || free_ != nullptr; });
        if (closed_)
            return {};
        return take_available();
    }

    KZ_NODISCARD auto try_acquire() -> std::optional<UniquePtr> {
        std::lock_guard lock{mutex_};
        if (closed_ || free_ == nullptr)
            return std::nullopt;
        return take_available();
    }

    template <typename Rep, typename Period>
    KZ_NODISCARD auto acquire_for(std::chrono::duration<Rep, Period> timeout)
        -> std::optional<UniquePtr> {
        std::unique_lock lock{mutex_};
        if (!cv_.wait_for(lock, timeout, [this] { return closed_ || free_ != nullptr; }) || closed_)
            return std::nullopt;
        return take_available();
    }

    KZ_NODISCARD auto acquire_shared() -> SharedPtr { return SharedPtr{acquire()}; }

    KZ_NODISCARD auto available_count() const noexcept -> std::size_t {
        std::lock_guard lock{mutex_};
        return available_;
    }

    KZ_NODISCARD auto size() const noexcept -> std::size_t {
        std::lock_guard lock{mutex_};
        return entries_.size();
    }

    auto close() noexcept -> void {
        {
            std::lock_guard lock{mutex_};
            closed_ = true;
            free_ = nullptr;
            available_ = 0;
        }
        cv_.notify_all();
    }

private:
    KZ_NODISCARD auto take_available() -> UniquePtr {
        auto* entry = free_;
        free_ = entry->next;
        entry->next = nullptr;
        --available_;
        return UniquePtr{entry->object.get(), Deleter{this, entry}};
    }

    auto release(Entry* entry) noexcept -> void {
        {
            std::lock_guard lock{mutex_};
            if (closed_)
                return;
            entry->next = free_;
            free_ = entry;
            ++available_;
        }
        cv_.notify_one();
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::unique_ptr<Entry>> entries_;
    Entry* free_{};
    std::size_t available_{};
    bool closed_{};
};

}  // namespace kitzoo::thread
