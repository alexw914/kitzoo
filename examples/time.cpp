#include <kitzoo/time.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using namespace std::chrono_literals;
    kitzoo::time::Stopwatch watch;
    auto const deadline = kitzoo::time::Deadline::after(20ms);
    std::atomic<int> ticks{0};
    kitzoo::time::Timer timer{2ms};
    timer.start([&ticks] { ticks.fetch_add(1, std::memory_order_relaxed); });
    std::this_thread::sleep_for(8ms);
    timer.stop();
    std::printf("elapsed=%lldms deadline_expired=%s ticks=%d timestamp=%s\n",
                static_cast<long long>(watch.elapsed_as<std::chrono::milliseconds>().count()),
                deadline.expired() ? "yes" : "no", ticks.load(),
                kitzoo::time::format_timestamp().c_str());
}
