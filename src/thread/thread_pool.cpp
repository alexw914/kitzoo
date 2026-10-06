// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/thread/thread_pool.cpp
// Description: Implements worker creation, task scheduling, task completion,
//              and orderly shutdown for ThreadPool.
// -----------------------------------------------------------------------------

#include <kitzoo/os/sys.hpp>
#include <kitzoo/thread/thread_pool.hpp>

#include <new>
#include <string>

namespace kitzoo::thread {

namespace {
thread_local const ThreadPool* active_pool = nullptr;
} // namespace

ThreadPool::ThreadPool(std::size_t num_threads) : ThreadPool(num_threads, {}) {}

ThreadPool::ThreadPool(std::size_t num_threads, std::string_view name) {
  if (num_threads == 0) {
    num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
      num_threads = 4;
  }

  workers_.reserve(num_threads);
  for (std::size_t i = 0; i < num_threads; ++i) {
    auto worker_name = name.empty() ? std::string{} : std::string{name} + "-" + std::to_string(i);
    workers_.emplace_back([this, worker_name = std::move(worker_name)] {
      if (!worker_name.empty())
        os::set_current_thread_name(worker_name);
      worker_loop();
    });
  }
}

ThreadPool::~ThreadPool() {
  shutdown();
}

auto ThreadPool::get_thread_count() const noexcept -> std::size_t {
  return workers_.size();
}

auto ThreadPool::get_tasks_queued() const noexcept -> std::size_t {
  const auto running = running_.load(std::memory_order_acquire);
  const auto pending = pending_.load(std::memory_order_acquire);
  return pending > running ? pending - running : 0;
}

auto ThreadPool::get_tasks_running() const noexcept -> std::size_t {
  return running_.load(std::memory_order_acquire);
}

auto ThreadPool::get_tasks_total() const noexcept -> std::size_t {
  return pending_.load(std::memory_order_acquire);
}

auto ThreadPool::wait() -> void {
  if (active_pool == this)
    throw std::logic_error{"ThreadPool: cannot wait from a worker thread"};
  for (auto pending = pending_.load(); pending != 0; pending = pending_.load())
    pending_.wait(pending);
}

auto ThreadPool::finish_task() noexcept -> void {
  if (pending_.fetch_sub(1) == 1)
    pending_.notify_all();
}

// pending_ and accepting_ use sequentially consistent operations: a submission
// that sees accepting_ set has already raised pending_ before shutdown reads it.
auto ThreadPool::enqueue_task(Task task) -> void {
  pending_.fetch_add(1);
  if (!accepting_.load()) {
    finish_task();
    throw std::runtime_error{"ThreadPool: pool is shutting down"};
  }
  if (!tasks_.enqueue(std::move(task))) {
    finish_task();
    throw std::bad_alloc{};
  }
}

auto ThreadPool::shutdown() -> void {
  if (active_pool == this)
    throw std::logic_error{"ThreadPool: cannot shut down from a worker thread"};
  if (!accepting_.exchange(false))
    return;
  for (auto pending = pending_.load(); pending != 0; pending = pending_.load())
    pending_.wait(pending);
  for (std::size_t i = 0; i < workers_.size(); ++i)
    tasks_.enqueue(Task{});
  for (auto& worker : workers_) {
    if (worker.joinable())
      worker.join();
  }
}

auto ThreadPool::worker_loop() -> void {
  active_pool = this;
  for (;;) {
    Task task;
    tasks_.wait_dequeue(task);
    if (!task)
      return;
    running_.fetch_add(1, std::memory_order_relaxed);
    task();
    running_.fetch_sub(1, std::memory_order_relaxed);
    finish_task();
  }
}

} // namespace kitzoo::thread
