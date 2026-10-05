// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/time.cpp
// Description: Demonstrates calendar conversion, replay clocks and steady measurements.
// -----------------------------------------------------------------------------

#include <kitzoo/time.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <stop_token>
#include <thread>

namespace {
namespace time = kitzoo::time;
using namespace std::chrono_literals;

auto calendar_and_clocks() -> void {
  const auto now = time::utc_timestamp();
  std::cout << "UTC: " << time::format_timestamp(now, time::TimeZone::Utc, time::TimestampPrecision::Nanoseconds)
            << "\nlocal: " << time::format_timestamp(now) << '\n';
  // Calendar conversions use an explicit zone; UTC is independent of machine settings.
  const auto leap_day = time::from_date_time({2040, 2, 29, 12, 34, 56, 123456789});
  const auto date = time::to_date_time(leap_day);
  std::cout << "leap day: " << time::format_timestamp(leap_day, time::TimeZone::Utc) << " year day=" << date.day_of_year
            << " weekday=" << date.weekday << " round trip=" << (time::from_date_time(date) == leap_day) << '\n';
  // steady_timestamp has an unspecified epoch: use differences, not dates.
  const auto started = time::steady_timestamp();
  std::cout << "steady difference: " << (time::steady_timestamp() - started).count() << " ns\n";
  // Linux PTP is optional. Its configured hardware timescale need not equal UTC.
  if (const auto ptp = time::ptp_timestamp())
    std::cout << "PTP reading: " << ptp->count() << " ns\n";
  else
    std::cout << "PTP device unavailable\n";
}

auto stopwatch_and_deadlines() -> void {
  time::Stopwatch watch;
  const std::array<int, 4> samples{10, 20, 30, 40};
  const int total = std::accumulate(samples.begin(), samples.end(), 0);
  std::cout << "sample sum=" << total << " elapsed=" << watch.elapsed_as<std::chrono::nanoseconds>().count() << " ns\n";
  watch.reset();
  // Deadlines use the steady clock, so wall-clock changes do not shift them.
  const auto deadline = time::Deadline::after(1s);
  std::cout << "budget remaining="
            << std::chrono::duration_cast<std::chrono::milliseconds>(deadline.remaining()).count()
            << " ms, expired=" << deadline.expired() << '\n';
  const auto past = time::Deadline::at(std::chrono::steady_clock::now() - 1ms);
  std::cout << "past deadline expired=" << past.expired() << '\n';
}

auto replay_and_corrected_timelines() -> void {
  auto replay = std::make_shared<time::FeederTimeline>();
  std::cout << "replay valid before first sample=" << replay->is_valid() << '\n';
  replay->feed(0ns); // Zero is a valid replay sample, not an invalid sentinel.
  std::jthread producer([replay] { replay->feed(10ms); });
  // An absolute target is safe even if the producer feeds before waiting starts.
  const bool reached = replay->sleep_until(10ms, 2s);
  producer.join();
  std::cout << "replay reached 10 ms=" << reached << '\n';
  // A paused replay uses a real steady timeout; zero timeout means unlimited.
  std::cout << "paused replay timed out=" << !replay->sleep_for(1s, 5ms) << '\n';
  std::stop_source cancellation;
  cancellation.request_stop();
  std::cout << "cancelled wait completed=" << replay->sleep_for(1s, {}, cancellation.get_token()) << '\n';

  time::OffsetTimeline corrected(replay, -2ms);
  std::cout << "corrected replay=" << corrected.timestamp().count() << " ns\n";
  time::CallbackTimeline devices(
      [](std::string_view key) -> time::TimeDuration { return key == "camera" ? 42ms : 7ms; });
  std::cout << "camera=" << devices.timestamp("camera").count() << " ns\n";

  // Global selection is optional and one-shot: select before its first query.
  // Independent timeline objects remain usable for other streams.
  auto& service = time::Time::instance();
  if (!service.init(replay))
    throw std::runtime_error("global timeline was already selected");
  std::cout << "global replay=" << service.timestamp().count() << " ns\n";
}

auto named_and_scoped_measurements() -> void {
  time::TimeWatcher watcher;
  watcher.begin("decode");
  const auto decoded = watcher.end("decode");
  std::cout << "decode=" << decoded.count() << " ns, cached=" << (watcher.last_result("decode") == decoded) << '\n';
  {
    auto scope = watcher.scope("frame", [&](std::string_view name, time::TimeDuration elapsed) {
      // Completion caches the result before invoking this callback outside locks.
      std::cout << name << '=' << elapsed.count() << " ns, cached=" << (watcher.last_result(name) == elapsed) << '\n';
    });
    std::cout << "processing frame\n";
  } // RAII completion also runs during exception unwinding.
  auto scope = watcher.scope("explicit");
  const auto finished = scope.finish();
  std::cout << "explicit completion=" << finished.count() << " ns, repeated finish=" << (scope.finish() == finished)
            << '\n';
}

auto periodic_callbacks() -> void {
  std::promise<void> three_ticks;
  auto completed = three_ticks.get_future();
  std::atomic<int> ticks{0};
  time::Timer timer(2ms);
  timer.start([&] {
    if (ticks.fetch_add(1) == 2)
      three_ticks.set_value();
  });
  const auto status = completed.wait_for(5s);
  // Stop from the owning thread, never from the callback; it joins the worker.
  timer.stop();
  if (status != std::future_status::ready)
    throw std::runtime_error("timer callback timed out");
  std::cout << "timer ticks=" << ticks.load() << " running=" << timer.running() << '\n';
  // A stopped timer can restart with another callback. Destruction also stops it.
}
} // namespace

auto main() -> int {
  try {
    std::cout << std::boolalpha;
    calendar_and_clocks();
    stopwatch_and_deadlines();
    replay_and_corrected_timelines();
    named_and_scoped_measurements();
    periodic_callbacks();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "time example failed: " << error.what() << '\n';
    return 1;
  }
}
