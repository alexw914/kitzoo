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
#include <optional>
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

  template <typename F, typename... Args>
  using TaskResult = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

  // Detached tasks must not throw: an escaping exception terminates the process.
  // submit_task reports exceptions through its future. Both throw runtime_error
  // once shutdown has started.
  template <typename F, typename... Args>
  auto detach_task(F&& f, Args&&... args) -> void;

  template <typename F, typename... Args>
  auto submit_task(F&& f, Args&&... args) -> std::future<TaskResult<F, Args...>>;

  // Return false or nullopt once shutdown has started, so tasks can resubmit
  // themselves safely. Arguments are consumed either way.
  template <typename F, typename... Args>
  KZ_NODISCARD auto try_detach_task(F&& f, Args&&... args) -> bool;

  template <typename F, typename... Args>
  KZ_NODISCARD auto try_submit_task(F&& f, Args&&... args) -> std::optional<std::future<TaskResult<F, Args...>>>;

  // wait() and shutdown() throw logic_error when called from a worker thread.
  auto wait() -> void;

  KZ_NODISCARD auto get_thread_count() const noexcept -> std::size_t;
  KZ_NODISCARD auto get_tasks_queued() const noexcept -> std::size_t;
  KZ_NODISCARD auto get_tasks_running() const noexcept -> std::size_t;
  KZ_NODISCARD auto get_tasks_total() const noexcept -> std::size_t;

  auto shutdown() -> void;

private:
  using Task = kitzoo::core::unique_function<void()>;

  template <typename F, typename... Args>
  static auto bind_task(F&& f, Args&&... args) {
    return [fn = std::forward<F>(f), ... params = std::forward<Args>(args)]() mutable -> TaskResult<F, Args...> {
      return std::invoke(std::move(fn), std::move(params)...);
    };
  }

  KZ_NORETURN static auto throw_shutting_down() -> void;
  auto try_enqueue_task(Task task) -> bool;
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
  if (!try_detach_task(std::forward<F>(f), std::forward<Args>(args)...))
    throw_shutting_down();
}

template <typename F, typename... Args>
auto ThreadPool::submit_task(F&& f, Args&&... args) -> std::future<TaskResult<F, Args...>> {
  std::packaged_task<TaskResult<F, Args...>()> task{bind_task(std::forward<F>(f), std::forward<Args>(args)...)};
  auto future = task.get_future();
  if (!try_enqueue_task([t = std::move(task)]() mutable { t(); }))
    throw_shutting_down();
  return future;
}

template <typename F, typename... Args>
auto ThreadPool::try_detach_task(F&& f, Args&&... args) -> bool {
  return try_enqueue_task(bind_task(std::forward<F>(f), std::forward<Args>(args)...));
}

template <typename F, typename... Args>
auto ThreadPool::try_submit_task(F&& f, Args&&... args) -> std::optional<std::future<TaskResult<F, Args...>>> {
  std::packaged_task<TaskResult<F, Args...>()> task{bind_task(std::forward<F>(f), std::forward<Args>(args)...)};
  auto future = task.get_future();
  if (!try_enqueue_task([t = std::move(task)]() mutable { t(); }))
    return std::nullopt;
  return future;
}

} // namespace kitzoo::thread

#endif // KITZOO_THREAD_THREAD_POOL_HPP
