// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/thread_pool.hpp
// Description: Declares the built-in worker pool and re-exports BS thread pool types.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_THREAD_POOL_HPP
#define KITZOO_THREAD_THREAD_POOL_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/core/unique_function.hpp>
#include <kitzoo/memory/memory.hpp>

#include <BS_thread_pool.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace kitzoo::thread {

using BSLightThreadPool = BS::light_thread_pool;
using BSPriorityThreadPool = BS::priority_thread_pool;
using BSPauseThreadPool = BS::pause_thread_pool;
using BSWdcThreadPool = BS::wdc_thread_pool;

class ThreadPool {
public:
  explicit ThreadPool(std::size_t num_threads = 0);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  ThreadPool(ThreadPool&&) = delete;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;

  template <typename F, typename... Args>
  auto detach_task(F&& f, Args&&... args) -> void;

  template <typename F, typename... Args>
  auto submit_task(F&& f, Args&&... args) -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

  // wait() and shutdown() throw logic_error when called from a worker thread.
  auto wait() -> void;

  KZ_NODISCARD auto get_thread_count() const noexcept -> std::size_t;
  KZ_NODISCARD auto get_tasks_queued() const noexcept -> std::size_t;
  KZ_NODISCARD auto get_tasks_running() const noexcept -> std::size_t;
  KZ_NODISCARD auto get_tasks_total() const noexcept -> std::size_t;

  auto shutdown() -> void;

private:
  auto enqueue_task(kitzoo::core::unique_function<void()> task) -> void;
  auto worker_loop() -> void;

  memory::Vector<std::jthread> workers_;
  memory::Queue<kitzoo::core::unique_function<void()>> tasks_;
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

} // namespace kitzoo::thread

#endif // KITZOO_THREAD_THREAD_POOL_HPP
