// ---------------------------------------------------------------------------
// kitzoo example: ConcurrentQueue and BS::thread_pool integrations
// ---------------------------------------------------------------------------

#include <kitzoo/queue/concurrent_queue.hpp>
#include <kitzoo/thread/bs_thread_pool.hpp>

#include <cstdio>

int main() {
    kitzoo::queue::ConcurrentQueue<int> queue;
    kitzoo::thread::BSLightThreadPool pool{2};

    auto task = pool.submit_task([&queue] { queue.enqueue(42); });
    task.get();

    int value = 0;
    if (queue.try_dequeue(value))
        std::printf("queued value: %d\n", value);
}
