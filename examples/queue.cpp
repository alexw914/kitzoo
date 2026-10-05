// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/queue.cpp
// Description: Demonstrates blocking, single-producer, and concurrent queues.
// -----------------------------------------------------------------------------

#include <kitzoo/queue.hpp>

#include <chrono>
#include <cstdio>
#include <thread>

using namespace kitzoo;
using namespace kitzoo::queue;

auto main() -> int {
  // BlockingQueue: two producers feed one consumer; close() ends the stream.
  // The capacity bounds memory: producers block while 16 items are pending.
  {
    BlockingQueue<int> queue{16};

    std::jthread consumer([&queue] {
      int sum = 0;
      while (const auto item = queue.wait_and_pop())
        sum += *item;
      std::printf("blocking queue sum: %d\n", sum);
    });

    std::jthread p1([&queue] {
      for (int i = 1; i <= 50; ++i)
        queue.push(i);
    });
    std::jthread p2([&queue] {
      for (int i = 51; i <= 100; ++i)
        queue.push(i);
    });

    p1.join();
    p2.join();
    queue.close(); // consumer drains, then sees std::nullopt and exits
  }

  // pop_for waits with a timeout instead of blocking indefinitely.
  {
    BlockingQueue<int> queue;
    const auto item = queue.pop_for(std::chrono::milliseconds{10});
    std::printf("pop_for on an empty queue: %s\n", item ? "item" : "timed out");
  }

  // SPSCQueue: one producer thread, one consumer thread, no locks.
  {
    SPSCQueue<int, 1024> queue;
    constexpr int kCount = 100000;

    std::jthread producer([&queue] {
      for (int i = 1; i <= kCount; ++i) {
        while (!queue.push(i))
          std::this_thread::yield(); // full — retry
      }
    });

    long long sum = 0;
    for (int seen = 0; seen < kCount;) {
      if (const auto item = queue.pop()) {
        sum += *item;
        ++seen;
      } else {
        std::this_thread::yield(); // empty — wait for producer
      }
    }
    producer.join();
    std::printf("spsc queue sum: %lld\n", sum);
  }

  // ConcurrentQueue supports multiple producers and consumers.
  {
    ConcurrentQueue<int> queue;
    queue.enqueue(42);
    int value = 0;
    if (queue.try_dequeue(value))
      std::printf("concurrent queue value: %d\n", value);
  }

  return 0;
}
