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
#include <kitzoo/queue/concurrent_queue.hpp>

#include <BS_thread_pool.hpp>
#include <atomic>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

namespace kitzoo::thread {

using BSLightThreadPool = BS::light_thread_pool;
using BSPriorityThreadPool = BS::priority_thread_pool;
using BSPauseThreadPool = BS::pause_thread_pool;
using BSWdcThreadPool = BS::wdc_thread_pool;

// Tasks wait in a lock-free queue and idle workers sleep on its semaphore.
// Tasks from one submitting thread start in order; tasks from different
// submitting threads have no relative order.
class ThreadPool {
public:
  explicit ThreadPool(std::size_t num_threads = 0);

  // Workers are named <name>-<index> under os::kThreadNamePrefix where supported.
  ThreadPool(std::size_t num_threads, std::string_view name);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  ThreadPool(ThreadPool&&) = delete;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;

  // Detached tasks must not throw: an escaping exception terminates the process.
  // submit_task reports exceptions through its future.
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
  using Task = kitzoo::core::unique_function<void()>;

  auto enqueue_task(Task task) -> void;
  auto finish_task() noexcept -> void;
  auto worker_loop() -> void;

  // An empty Task is the stop signal; shutdown enqueues one per worker.
  queue::BlockingConcurrentQueue<Task> tasks_;
  // Queued plus running tasks; submit and shutdown order through it so that a
  // racing submission is either rejected or completed before workers stop.
  std::atomic<std::size_t> pending_{0};
  std::atomic<std::size_t> running_{0};
  std::atomic<bool> accepting_{true};
  memory::Vector<std::jthread> workers_;
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
