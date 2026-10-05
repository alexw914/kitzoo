// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/thread.cpp
// Description: Demonstrates task execution, object pool leases and synchronized values.
// -----------------------------------------------------------------------------

#include <kitzoo/thread.hpp>

#include <cstdio>
#include <stdexcept>

auto main() -> int {
  // Synchronized access.
  kitzoo::thread::Synchronized<int, kitzoo::thread::SpinLock> counter{0};
  counter.with_lock([](int& value) -> void { ++value; });
  std::printf("synchronized value: %d\n", counter.copy());

  // Task submission and returned values.
  kitzoo::thread::BSLightThreadPool pool{2};
  auto task = pool.submit_task([]() -> int { return 42; });
  std::printf("task result: %d\n", task.get());

  // kitzoo ThreadPool returns task exceptions through the future.
  kitzoo::thread::ThreadPool workers{2};
  auto failed = workers.submit_task([]() -> int { throw std::runtime_error{"sensor offline"}; });
  try {
    static_cast<void>(failed.get());
  } catch (const std::exception& e) {
    std::printf("submitted task failed: %s\n", e.what());
  }

  // Local object reuse and shared lease ownership.
  kitzoo::thread::LocalObjectPool<int> objects{1};
  auto lease = objects.acquire_shared(7);
  kitzoo::memory::WeakPtr<int> weak = lease;
  std::printf("pooled value: %d, live objects: %zu\n", *lease, objects.allocated_count());
  lease.reset(); // Return the slot through the pool's deleter.
  auto reused = objects.acquire(42);
  std::printf("reused value: %d, previous weak reference expired: %s\n", *reused, weak.expired() ? "yes" : "no");
  weak.reset();

  // Concurrent pool leases.
  kitzoo::thread::ObjectPool<int> concurrent;
  concurrent.add(kitzoo::memory::make_unique<int>(99));
  auto shared = concurrent.acquire_shared();
  std::printf("concurrent pooled value: %d\n", *shared);
  shared.reset();
  std::printf("objects returned to concurrent pool: %zu\n", concurrent.available_approx());
  return 0;
}
