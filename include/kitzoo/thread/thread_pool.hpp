// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/thread_pool.hpp
// Description: Declares the fixed-size worker pool, task submission API, and
//              orderly shutdown behavior.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_THREAD_POOL_HPP
#define KITZOO_THREAD_THREAD_POOL_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/utilities/unique_function.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace kitzoo::thread {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads = 0);
    ~ThreadPool();

    ThreadPool(ThreadPool const&) = delete;
    auto operator=(ThreadPool const&) -> ThreadPool& = delete;
    ThreadPool(ThreadPool&&) = delete;
    auto operator=(ThreadPool&&) -> ThreadPool& = delete;

    template <typename F, typename... Args>
    auto detach_task(F&& f, Args&&... args) -> void;

    template <typename F, typename... Args>
    auto submit_task(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

    auto wait() -> void;

    KZ_NODISCARD auto get_thread_count() const noexcept -> std::size_t;
    KZ_NODISCARD auto get_tasks_queued() const noexcept -> std::size_t;
    KZ_NODISCARD auto get_tasks_running() const noexcept -> std::size_t;
    KZ_NODISCARD auto get_tasks_total() const noexcept -> std::size_t;

    auto shutdown() -> void;

private:
    auto enqueue_task(kitzoo::util::unique_function<void()> task) -> void;
    auto worker_loop() -> void;

    std::vector<std::jthread> workers_;
    std::queue<kitzoo::util::unique_function<void()>> tasks_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    mutable std::condition_variable tasks_done_cv_;
    std::size_t running_tasks_{0};
    std::atomic<bool> accepting_{true};
};

template <typename F, typename... Args>
auto ThreadPool::detach_task(F&& f, Args&&... args) -> void {
    enqueue_task([fn = std::forward<F>(f), ... params = std::forward<Args>(args)]() mutable {
        std::invoke(std::move(fn), std::move(params)...);
    });
}

template <typename F, typename... Args>
auto ThreadPool::submit_task(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using result_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    auto task = std::packaged_task<result_type()>{
        [fn = std::forward<F>(f), ... params = std::forward<Args>(args)]() mutable -> result_type {
            return std::invoke(std::move(fn), std::move(params)...);
        }};

    auto future = task.get_future();
    enqueue_task([t = std::move(task)]() mutable { t(); });
    return future;
}

}  // namespace kitzoo::thread

#endif  // KITZOO_THREAD_THREAD_POOL_HPP
