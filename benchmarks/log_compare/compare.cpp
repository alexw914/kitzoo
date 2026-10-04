// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: benchmarks/log_compare/compare.cpp
// Description: Measures reconstructed mlog and spdlog delivery pipelines.
// -----------------------------------------------------------------------------
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mlog_core/mlog.h>
#include <mutex>
#include <spdlog/async.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

class CountingSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    std::atomic<std::size_t> count{0};

protected:
    auto sink_it_(const spdlog::details::log_msg& message) -> void override {
        spdlog::memory_buf_t formatted;
        formatter_->format(message, formatted);
        count.fetch_add(1, std::memory_order_relaxed);
    }

    auto flush_() -> void override {}
};

struct Result {
    double submit;
    double complete;
    std::size_t delivered;
};

auto run(bool use_mlog, bool async, bool filtered, int producers) -> Result {
    constexpr int per_thread = 20000;
    constexpr int batch = 1000;
    std::atomic<std::size_t> count{0};
    auto sink = std::make_shared<CountingSink>();
    std::shared_ptr<mlog::MLogManager> ml;
    std::shared_ptr<spdlog::logger> sp;
    std::shared_ptr<spdlog::details::thread_pool> pool;
    if (use_mlog) {
        mlog::MLogSettings settings{};
        settings.endpoint = mlog::MLOG_ENDPOINT_CUSTOM_CALLBACK;
        settings.enable_background_dump = async;
        settings.level = filtered ? mlog::MLOG_LEVEL_ERROR : mlog::MLOG_LEVEL_INFO;
        settings.data_entry_limits = 100;
        settings.endpoint_callback = [&](const auto& entries) {
            count.fetch_add(entries.size(), std::memory_order_relaxed);
        };
        ml = mlog::MLogManager::make(settings);
    } else {
        if (async) {
            pool = std::make_shared<spdlog::details::thread_pool>(8192 * producers, 1);
            sp = std::make_shared<spdlog::async_logger>("bench", sink, pool,
                                                        spdlog::async_overflow_policy::block);
        } else {
            sp = std::make_shared<spdlog::logger>("bench", sink);
        }
        sp->set_level(filtered ? spdlog::level::err : spdlog::level::info);
        sp->set_pattern("[%l][%E.%F][%Y-%m-%d %H:%M:%S.%F]:%v");
    }
    if (filtered) {
        constexpr int calls = 2000000;
        auto begin = Clock::now();
        for (int i = 0; i < calls; ++i) {
            if (use_mlog)
                ml->record_log(mlog::MLOG_LEVEL_INFO, "record=%d", i);
            else
                sp->info("record={}", i);
        }
        auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - begin).count();
        if (count.load() || sink->count.load())
            std::abort();
        return {elapsed / calls, elapsed / calls, 0};
    }
    double submitted = 0;
    double total = 0;
    // Bounded batches prevent mlog overflow. Drain cost is included separately.
    for (int base = 0; base < per_thread; base += batch) {
        std::atomic<int> ready{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;
        for (int t = 0; t < producers; ++t) {
            threads.emplace_back([&, t] {
                ready.fetch_add(1);
                while (!start.load(std::memory_order_acquire))
                    std::this_thread::yield();
                for (int i = base; i < base + batch; ++i) {
                    if (use_mlog)
                        ml->record_log(mlog::MLOG_LEVEL_INFO,
                                       "record=%d worker=%d value=%.3f status=%s", i, t, 42.125,
                                       "ready");
                    else
                        sp->info("record={} worker={} value={:.3f} status={}", i, t, 42.125,
                                 "ready");
                }
            });
        }
        while (ready.load() != producers)
            std::this_thread::yield();
        auto begin = Clock::now();
        start.store(true, std::memory_order_release);
        for (auto& thread : threads)
            thread.join();
        auto end_submit = Clock::now();
        if (use_mlog)
            ml->flush_log(true);
        else if (async && !filtered) {
            auto expected = std::size_t(base + batch) * producers;
            auto deadline = Clock::now() + std::chrono::seconds(10);
            while (sink->count.load() < expected) {
                if (Clock::now() > deadline)
                    std::abort();
                std::this_thread::yield();
            }
        }
        auto end = Clock::now();
        submitted += std::chrono::duration<double, std::nano>(end_submit - begin).count();
        total += std::chrono::duration<double, std::nano>(end - begin).count();
    }
    auto delivered = use_mlog ? count.load() : sink->count.load();
    if (delivered != (filtered ? 0 : std::size_t(per_thread) * producers))
        std::abort();
    if (ml)
        ml->stop();
    auto n = double(per_thread * producers);
    return {submitted / n, total / n, delivered};
}

auto main() -> int {
    setenv("MTIME_DISABLE_LOCAL_TIME", "ON", 1);
    std::cout << "case,library,submit_ns_per_call,complete_ns_per_call,delivered\n";
    for (auto mode : {0, 1, 2, 3}) {
        bool filtered = mode == 0;
        bool async = mode >= 2;
        int producers = mode == 3 ? 4 : 1;
        std::vector<Result> values[2];
        for (int repeat = 0; repeat < 8; ++repeat) {
            for (int order = 0; order < 2; ++order) {
                int lib = (repeat + order) % 2;
                auto result = run(lib == 0, async, filtered, producers);
                if (repeat)
                    values[lib].push_back(result);
            }
        }
        for (int lib = 0; lib < 2; ++lib) {
            std::vector<double> submit, complete;
            for (const auto& result : values[lib]) {
                submit.push_back(result.submit);
                complete.push_back(result.complete);
            }
            std::sort(submit.begin(), submit.end());
            std::sort(complete.begin(), complete.end());
            std::cout << (filtered ? "filtered"
                          : async  ? producers == 4 ? "async4" : "async1"
                                   : "sync1")
                      << ',' << (lib == 0 ? "mlog" : "spdlog") << ',' << submit[3] << ','
                      << complete[3] << ',' << values[lib][0].delivered << '\n';
        }
    }
}
