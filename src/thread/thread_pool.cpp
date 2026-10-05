// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/thread/thread_pool.cpp
// Description: Implements worker creation, task scheduling, task completion,
//              and orderly shutdown for ThreadPool.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/thread_pool.hpp>

namespace kitzoo::thread {

namespace {
thread_local const ThreadPool* active_pool = nullptr;
} // namespace

ThreadPool::ThreadPool(std::size_t num_threads) {
  if (num_threads == 0) {
    num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
      num_threads = 4;
  }

  workers_.reserve(num_threads);
  for (std::size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  shutdown();
}

auto ThreadPool::get_thread_count() const noexcept -> std::size_t {
  return workers_.size();
}

auto ThreadPool::get_tasks_queued() const noexcept -> std::size_t {
  std::lock_guard lock{mutex_};
  return tasks_.size();
}

auto ThreadPool::get_tasks_running() const noexcept -> std::size_t {
  std::lock_guard lock{mutex_};
  return running_tasks_;
}

auto ThreadPool::get_tasks_total() const noexcept -> std::size_t {
  std::lock_guard lock{mutex_};
  return tasks_.size() + running_tasks_;
}

auto ThreadPool::wait() -> void {
  if (active_pool == this)
    throw std::logic_error{"ThreadPool: cannot wait from a worker thread"};
  std::unique_lock lock{mutex_};
  tasks_done_cv_.wait(lock, [this] { return tasks_.empty() && running_tasks_ == 0; });
}

auto ThreadPool::enqueue_task(kitzoo::core::unique_function<void()> task) -> void {
  {
    std::lock_guard lock{mutex_};
    if (!accepting_.load(std::memory_order_acquire)) {
      throw std::runtime_error{"ThreadPool: pool is shutting down"};
    }
    tasks_.emplace(std::move(task));
  }
  cv_.notify_one();
}

auto ThreadPool::shutdown() -> void {
  if (active_pool == this)
    throw std::logic_error{"ThreadPool: cannot shut down from a worker thread"};
  {
    std::lock_guard lock{mutex_};
    accepting_.store(false, std::memory_order_release);
  }
  cv_.notify_all();

  for (auto& worker : workers_) {
    if (worker.joinable())
      worker.join();
  }
}

auto ThreadPool::worker_loop() -> void {
  active_pool = this;
  while (true) {
    kitzoo::core::unique_function<void()> task;
    {
      std::unique_lock lock{mutex_};
      cv_.wait(lock, [this] { return !tasks_.empty() || !accepting_.load(std::memory_order_acquire); });

      if (tasks_.empty() && !accepting_.load(std::memory_order_acquire))
        return;

      task = std::move(tasks_.front());
      tasks_.pop();
      ++running_tasks_;
    }
    task();
    {
      std::lock_guard lock{mutex_};
      --running_tasks_;
      if (tasks_.empty() && running_tasks_ == 0)
        tasks_done_cv_.notify_all();
    }
  }
}

} // namespace kitzoo::thread
