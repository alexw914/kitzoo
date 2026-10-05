// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/thread.cpp
// Description: Demonstrates task execution, object pool leases and synchronized values.
// -----------------------------------------------------------------------------

#include <kitzoo/queue/concurrent_queue.hpp>
#include <kitzoo/thread.hpp>

#include <cstdio>

auto main() -> int {
  kitzoo::thread::Synchronized<int, kitzoo::thread::SpinLock> counter{0};
  counter.with_lock([](int& value) -> void { ++value; });
  std::printf("synchronized value: %d\n", counter.copy());

  kitzoo::queue::ConcurrentQueue<int> queue;
  kitzoo::thread::BSLightThreadPool pool{2};

  auto task = pool.submit_task([&queue] { queue.enqueue(42); });
  task.get();

  int value = 0;
  if (queue.try_dequeue(value))
    std::printf("queued value: %d\n", value);

  kitzoo::thread::ObjectPool<int> objects{1};
  auto lease = objects.acquire_shared(7);
  kitzoo::memory::WeakPtr<int> weak = lease;
  std::printf("pooled value: %d, live objects: %zu\n", *lease, objects.allocated_count());
  lease.reset(); // Return the slot through the pool's deleter.
  auto reused = objects.acquire(42);
  std::printf("reused value: %d, previous weak reference expired: %s\n", *reused, weak.expired() ? "yes" : "no");
  weak.reset();

  kitzoo::thread::ConcurrentObjectPool<int> concurrent;
  concurrent.add(kitzoo::memory::make_unique<int>(99));
  auto shared = concurrent.acquire_shared();
  std::printf("concurrent pooled value: %d\n", *shared);
  shared.reset();
  std::printf("objects returned to concurrent pool: %zu\n", concurrent.available_approx());
  return 0;
}
