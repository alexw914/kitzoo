// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/queue/blocking_queue.hpp
// Description: Declares a closable multi-producer, multi-consumer queue whose
//              consumers can wait for items.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace kitzoo::queue {

template <typename T>
class BlockingQueue {
public:
    BlockingQueue() = default;
    ~BlockingQueue() = default;

    BlockingQueue(BlockingQueue const&) = delete;
    auto operator=(BlockingQueue const&) -> BlockingQueue& = delete;
    BlockingQueue(BlockingQueue&&) = delete;
    auto operator=(BlockingQueue&&) -> BlockingQueue& = delete;

    auto push(T value) -> bool {
        {
            std::lock_guard lock{mutex_};
            if (closed_)
                return false;
            deque_.push_back(std::move(value));
        }
        cv_.notify_one();
        return true;
    }

    KZ_NODISCARD auto wait_and_pop() -> std::optional<T> {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return !deque_.empty() || closed_; });
        if (deque_.empty())
            return std::nullopt;
        auto value = std::move(deque_.front());
        deque_.pop_front();
        return value;
    }

    KZ_NODISCARD auto try_pop() -> std::optional<T> {
        std::lock_guard lock{mutex_};
        if (deque_.empty())
            return std::nullopt;
        auto value = std::move(deque_.front());
        deque_.pop_front();
        return value;
    }

    auto close() -> void {
        {
            std::lock_guard lock{mutex_};
            closed_ = true;
        }
        cv_.notify_all();
    }

    KZ_NODISCARD auto is_closed() const -> bool {
        std::lock_guard lock{mutex_};
        return closed_;
    }

    KZ_NODISCARD auto empty() const -> bool {
        std::lock_guard lock{mutex_};
        return deque_.empty();
    }

    KZ_NODISCARD auto size() const -> std::size_t {
        std::lock_guard lock{mutex_};
        return deque_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> deque_;
    bool closed_{false};
};

}  // namespace kitzoo::queue
