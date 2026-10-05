// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/time.cpp
// Description: Demonstrates clocks, calendars, measurements, timelines and periodic callbacks.
// -----------------------------------------------------------------------------

#include <kitzoo/time.hpp>

#include <algorithm>
#include <atomic>
#include <exception>
#include <future>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;

// Case 1: Read clocks and convert calendar dates.
auto clocks_and_calendar() -> void {
  const auto now = kitzoo::time::utc_timestamp();
  std::cout << "UTC: " << kitzoo::time::format_timestamp(now, kitzoo::time::TimeZone::Utc)
            << "\nlocal: " << kitzoo::time::format_timestamp(now) << '\n';
  const auto timestamp = kitzoo::time::from_date_time({2040, 2, 29, 12, 34, 56, 123456789});
  const auto date = kitzoo::time::to_date_time(timestamp);
  std::cout << "leap day: " << kitzoo::time::format_timestamp(timestamp, kitzoo::time::TimeZone::Utc)
            << " year day=" << date.day_of_year << '\n';
  // A monotonic epoch and a PTP device timescale must not be interpreted as UTC dates.
  const auto started = kitzoo::time::steady_timestamp();
  std::cout << "steady difference: " << (kitzoo::time::steady_timestamp() - started).count() << " ns\n";
  if (const auto ptp = kitzoo::time::ptp_timestamp())
    std::cout << "PTP reading: " << ptp->count() << " ns\n";
  else
    std::cout << "PTP device unavailable\n";
}

// Case 2: Measure tasks, track a timeout budget and cache named measurements.
auto task_measurements() -> void {
  std::vector<int> samples(10000);
  kitzoo::time::Stopwatch watch;
  std::iota(samples.begin(), samples.end(), 0);
  const auto total = std::accumulate(samples.begin(), samples.end(), 0LL);
  std::cout << "sample sum=" << total << " elapsed=" << watch.elapsed_as<std::chrono::microseconds>().count()
            << " us\n";
  const auto budget = kitzoo::time::Deadline::after(1s);
  std::cout << "budget remaining=" << std::chrono::duration_cast<std::chrono::milliseconds>(budget.remaining()).count()
            << " ms\n";

  kitzoo::time::TimeWatcher watcher;
  watcher.begin("decode");
  std::reverse(samples.begin(), samples.end());
  std::cout << "decode=" << watcher.end("decode").count() << " ns\n";
  {
    auto scope = watcher.scope("frame", [](std::string_view name, kitzoo::time::TimeDuration elapsed) -> void {
      std::cout << name << '=' << elapsed.count() << " ns\n";
    });
    std::partial_sum(samples.begin(), samples.end(), samples.begin());
  }
  std::cout << "cached frame=" << watcher.last_result("frame")->count() << " ns\n";
}

// Case 3: Follow replay progress, read device clocks and apply clock correction.
auto replay_timelines() -> void {
  auto replay = std::make_shared<kitzoo::time::FeederTimeline>();
  replay->feed(0ns);
  std::jthread producer([replay]() -> void { replay->feed(10ms); });
  // An absolute target also works when the producer advances before the wait begins.
  const bool reached = replay->sleep_until(10ms, 2s);
  producer.join();
  if (!reached)
    throw std::runtime_error("replay target timed out");
  std::cout << "replay reached 10 ms\n";

  kitzoo::time::OffsetTimeline corrected(replay, -2ms);
  std::cout << "corrected replay=" << corrected.timestamp().count() << " ns\n";
  kitzoo::time::CallbackTimeline device(
      [](std::string_view key) -> kitzoo::time::TimeDuration { return key == "camera" ? 42ms : 7ms; });
  std::cout << "camera=" << device.timestamp("camera").count() << " ns\n";

  // Optional global selection must happen before the first service query.
  auto& service = kitzoo::time::Time::instance();
  if (!service.init(replay))
    throw std::runtime_error("global timeline was already selected");
  std::cout << "global replay=" << service.timestamp().count() << " ns\n";
}

// Case 4: Run periodic callbacks and stop the worker from its owning thread.
auto periodic_callbacks() -> void {
  std::promise<void> three_ticks;
  auto completed = three_ticks.get_future();
  std::atomic<int> ticks{0};
  kitzoo::time::Timer timer(2ms);
  timer.start([&]() -> void {
    if (ticks.fetch_add(1) == 2)
      three_ticks.set_value();
  });
  const auto status = completed.wait_for(5s);
  // stop joins the worker: call it from the owning thread, outside the callback.
  timer.stop();
  if (status != std::future_status::ready) {
    throw std::runtime_error("timer callback timed out");
  }
  std::cout << "timer ticks=" << ticks.load() << '\n';
}
} // namespace

auto main() -> int {
  try {
    clocks_and_calendar();
    task_measurements();
    replay_timelines();
    periodic_callbacks();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "time example failed: " << error.what() << '\n';
    return 1;
  }
}
