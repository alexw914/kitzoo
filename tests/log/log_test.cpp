// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/log/log_test.cpp
// Description: Verifies logging records, configuration, bounded delivery and errors.
// -----------------------------------------------------------------------------

#include <kitzoo/log.hpp>
#include <kitzoo/os/filesys.hpp>
#include <kitzoo/time/time.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <set>
#include <spdlog/details/os.h>
#include <spdlog/sinks/base_sink.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Throws if a filtered call accidentally reaches formatting.
struct FormatProbe {};

template <>
struct fmt::formatter<FormatProbe> : fmt::formatter<int> {
  template <typename Context>
  auto format(const FormatProbe&, Context& context) const -> decltype(context.out()) {
    throw std::runtime_error("unexpected formatting");
  }
};

namespace {
using namespace kitzoo::log;
using namespace std::chrono_literals;

struct CapturedRecord {
  std::string message;
  std::string formatted;
  std::string name;
  std::string file;
  int line;
  spdlog::level::level_enum level;
  std::chrono::system_clock::time_point timestamp;
};

class RecordingSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  std::atomic<int> flush_count{0};

  auto snapshot() -> std::vector<CapturedRecord> {
    std::lock_guard lock(mutex_);
    return records_;
  }

private:
  auto sink_it_(const spdlog::details::log_msg& message) -> void override {
    spdlog::memory_buf_t formatted;
    formatter_->format(message, formatted);
    records_.push_back(
        {std::string(message.payload.data(), message.payload.size()), std::string(formatted.data(), formatted.size()),
         std::string(message.logger_name.data(), message.logger_name.size()),
         message.source.filename ? message.source.filename : "", message.source.line, message.level, message.time});
  }

  auto flush_() -> void override { ++flush_count; }

  std::vector<CapturedRecord> records_;
};

class LogTest : public ::testing::Test {
protected:
  auto SetUp() -> void override {
    sink_ = std::make_shared<RecordingSink>();
    LoggerOptions options;
    options.pattern = "[%n][%l] %v";
    options.sinks = {sink_};
    logger_ = std::make_shared<Logger>("test", options);
  }

  std::shared_ptr<RecordingSink> sink_;
  std::shared_ptr<Logger> logger_;
};

class TemporaryDirectory {
public:
  TemporaryDirectory() : path(kitzoo::os::temp_directory()) {}

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  std::filesystem::path path;
};

// Holds the first output so queue overflow tests do not depend on scheduling.
// A finite callback wait ensures a failed assertion cannot hang destruction.
class OutputGate {
public:
  OutputGate() : entered_(entered_promise_.get_future()), release_(release_promise_.get_future().share()) {}

  auto pause_first() -> void {
    if (calls_.fetch_add(1) == 0) {
      entered_promise_.set_value();
      if (release_.wait_for(5s) != std::future_status::ready)
        timed_out_ = true;
    }
  }

  auto wait_until_entered() -> bool { return entered_.wait_for(5s) == std::future_status::ready; }

  auto release() -> void { release_promise_.set_value(); }

  auto timed_out() const -> bool { return timed_out_.load(); }

private:
  std::promise<void> entered_promise_;
  std::promise<void> release_promise_;
  std::future<void> entered_;
  std::shared_future<void> release_;
  std::atomic<int> calls_{0};
  std::atomic<bool> timed_out_{false};
};

TEST_F(LogTest, FormatsMessageAndPreservesSourceMetadata) {
  const auto location = std::source_location::current();
  logger_->logf(Level::Warn, location, "frame={} latency={:.2f}", 7, 1.25);
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].message, "frame=7 latency=1.25");
  EXPECT_EQ(records[0].name, "test");
  EXPECT_EQ(records[0].level, spdlog::level::warn);
  EXPECT_EQ(records[0].file, location.file_name());
  EXPECT_EQ(records[0].line, static_cast<int>(location.line()));
  EXPECT_NE(records[0].formatted.find("[test][warning] frame=7"), std::string::npos);
}

TEST_F(LogTest, RuntimeLevelChangesAndOffFilterRecords) {
  logger_->set_level(Level::Error);
  logger_->log(Level::Warn, "filtered");
  logger_->log(Level::Error, "error");
  logger_->set_level(Level::Trace);
  logger_->log(Level::Trace, "trace");
  logger_->set_level(Level::Off);
  logger_->log(Level::Fatal, "disabled");
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 2u);
  EXPECT_EQ(records[0].message, "error");
  EXPECT_EQ(records[1].message, "trace");
  EXPECT_FALSE(logger_->enabled(Level::Info));
}

TEST_F(LogTest, FilteredFormattingIsSkippedSynchronouslyAndAsynchronously) {
  logger_->set_level(Level::Error);
  EXPECT_NO_THROW(logger_->logf(Level::Debug, std::source_location::current(), "{}", FormatProbe{}));
  AsyncLogger async(logger_);
  EXPECT_NO_THROW(async.logf(Level::Debug, std::source_location::current(), "{}", FormatProbe{}));
  async.flush();
  EXPECT_TRUE(sink_->snapshot().empty());
  EXPECT_EQ(async.dropped_count(), 0u);
}

TEST(LogMacroTest, DisabledMacroDoesNotEvaluateArguments) {
  auto& logger = default_logger();
  logger.set_level(Level::Off);
  int evaluations = 0;
  KZ_LOG_INFO("{}", ++evaluations);
  logger.set_level(Level::Info);
  EXPECT_EQ(evaluations, 0);
}

TEST(LogMacroTest, CheckAbortsOnFailureInEveryBuildType) {
  // Runtime values avoid constant-condition warnings (MSVC C4127).
  const auto value = std::stoi("2");
  KZ_CHECK(value == 2);
  EXPECT_DEATH(KZ_CHECK(value == 3), "");
  EXPECT_DEATH(KZ_CHECK_MSG(value < 0, "custom {}", value), "");
}

TEST_F(LogTest, ExplicitTimestampIsPreservedWithoutTimezoneAssumptions) {
  const auto timestamp = std::chrono::system_clock::time_point{} + 48h + 123ms;
  logger_->log_at(timestamp, Level::Info, "replayed frame");
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].timestamp, timestamp);
  EXPECT_EQ(records[0].message, "replayed frame");
}

TEST_F(LogTest, BroadcastHonorsEachSinkLevelAndPatternChanges) {
  auto errors = std::make_shared<RecordingSink>();
  errors->set_level(spdlog::level::err);
  logger_->add_sink(errors);
  logger_->set_pattern("%v");
  logger_->log(Level::Info, "ordinary");
  logger_->log(Level::Error, "failure");
  ASSERT_EQ(sink_->snapshot().size(), 2u);
  const auto filtered = errors->snapshot();
  ASSERT_EQ(filtered.size(), 1u);
  EXPECT_EQ(filtered[0].message, "failure");
  EXPECT_EQ(filtered[0].formatted, sink_->snapshot()[1].formatted);
}

TEST_F(LogTest, FlushThresholdAndExplicitFlushReachSinks) {
  logger_->set_flush_level(Level::Error);
  logger_->log(Level::Info, "no automatic flush");
  EXPECT_EQ(sink_->flush_count.load(), 0);
  logger_->log(Level::Error, "automatic flush");
  EXPECT_EQ(sink_->flush_count.load(), 1);
  logger_->flush();
  EXPECT_EQ(sink_->flush_count.load(), 2);
}

TEST_F(LogTest, RejectsNullSink) {
  EXPECT_THROW(logger_->add_sink(nullptr), std::invalid_argument);
}

// Rolling files: naming, rollover, age cleanup and asynchronous delivery.
TEST(LogRollingFileTest, RejectsInvalidOptions) {
  FileSinkOptions options;
  EXPECT_THROW(RollingFileSink{options}, std::invalid_argument);
  TemporaryDirectory directory;
  options.path = directory.path / "application.log";
  options.max_size_bytes = 0;
  EXPECT_THROW(RollingFileSink{options}, std::invalid_argument);
  options.max_size_bytes = 64;
  options.max_age = -1s;
  EXPECT_THROW(RollingFileSink{options}, std::invalid_argument);
  options.max_age = 0s;
  options.cleanup_interval = -1ms;
  EXPECT_THROW(RollingFileSink{options}, std::invalid_argument);
}

TEST(LogRollingFileTest, OptionsCreateDirectoriesAndRotateWithoutRenamingOldFiles) {
  TemporaryDirectory directory;
  LoggerOptions options;
  options.pattern = "%v";
  options.file = FileSinkOptions{directory.path / "nested" / "application.log", 8, 0s, 0ms};
  Logger logger("file", options);
  logger.log(Level::Info, "first");
  logger.log(Level::Info, "second");
  logger.flush();
  std::vector<std::string> records;
  for (const auto& entry : std::filesystem::directory_iterator(directory.path / "nested")) {
    EXPECT_LE(entry.file_size(), 8u);
    records.push_back(kitzoo::os::read_file(entry.path()));
  }
  ASSERT_EQ(records.size(), 2u);
  EXPECT_TRUE(std::ranges::any_of(records, [](const std::string& text) -> bool { return text.starts_with("first"); }));
  EXPECT_TRUE(std::ranges::any_of(records, [](const std::string& text) -> bool { return text.starts_with("second"); }));
}

TEST(LogRollingFileTest, OversizedRecordStaysIntactAndNextRecordStartsNewFile) {
  TemporaryDirectory directory;
  Logger logger("large");
  logger.set_pattern("%v");
  auto sink = logger.add_file_sink({directory.path / "application.log", 8, 0s, 0ms});
  const auto first = sink->current_file();
  logger.log(Level::Info, std::string(100, 'x'));
  logger.flush();
  EXPECT_EQ(sink->current_file(), first);
  EXPECT_TRUE(kitzoo::os::read_file(first).starts_with(std::string(100, 'x')));
  logger.log(Level::Info, "next");
  logger.flush();
  EXPECT_NE(sink->current_file(), first);
  EXPECT_TRUE(std::filesystem::exists(first));
  EXPECT_TRUE(kitzoo::os::read_file(sink->current_file()).starts_with("next"));
}

TEST(LogRollingFileTest, ExactSizeBoundaryDoesNotSplitTheRecord) {
  TemporaryDirectory directory;
  const auto size = 4 + std::string_view(spdlog::details::os::default_eol).size();
  Logger logger("boundary");
  logger.set_pattern("%v");
  auto sink = logger.add_file_sink({directory.path / "application.log", size, 0s, 0ms});
  const auto first = sink->current_file();
  logger.log(Level::Info, "1234");
  logger.flush();
  EXPECT_EQ(sink->current_file(), first);
  EXPECT_EQ(std::filesystem::file_size(first), size);
  logger.log(Level::Info, "next");
  logger.flush();
  EXPECT_NE(sink->current_file(), first);
  EXPECT_EQ(std::filesystem::file_size(first), size);
}

TEST(LogRollingFileTest, NamesFilesWithTimestampAndKeepsHistoryAcrossRestart) {
  TemporaryDirectory directory;
  const FileSinkOptions options{directory.path / "application.log", 8, 0s, 0ms};
  auto count_files = [&directory]() -> std::size_t {
    return static_cast<std::size_t>(std::ranges::distance(std::filesystem::directory_iterator(directory.path)));
  };
  {
    Logger logger("first");
    logger.set_pattern("%v");
    auto sink = logger.add_file_sink(options);
    const auto name = sink->current_file().filename().string();
    // application_YYYYmmdd-HHMMSS_N.log
    ASSERT_EQ(name.size(), std::string_view("application_20261005-142530_0.log").size()) << name;
    EXPECT_TRUE(name.starts_with("application_") && name.ends_with("_0.log")) << name;
    EXPECT_EQ(name[20], '-');
    for (int index = 0; index < 3; ++index)
      logger.log(Level::Info, "entry");
    logger.flush();
  }
  EXPECT_EQ(count_files(), 3u);
  {
    Logger logger("restart");
    logger.set_pattern("%v");
    auto sink = logger.add_file_sink(options);
    logger.log(Level::Info, "newest");
    logger.flush();
    EXPECT_TRUE(kitzoo::os::read_file(sink->current_file()).starts_with("newest"));
  }
  EXPECT_EQ(count_files(), 4u);
}

TEST(LogRollingFileTest, AgeCleanupPreservesActiveAndUnrelatedFiles) {
  TemporaryDirectory directory;
  Logger logger("retention");
  logger.set_pattern("%v");
  auto sink = logger.add_file_sink({directory.path / "application.log", 8, 1h, 0ms});
  logger.log(Level::Info, "first");
  const auto closed = sink->current_file();
  logger.log(Level::Info, "second");
  logger.flush();
  const auto active = sink->current_file();
  const auto foreign = directory.path / "other.log";
  const auto malformed = directory.path / "application_20260101-000000_x.log";
  const auto subdirectory = directory.path / "application_20260101-000000_9.log";
  std::filesystem::create_directory(subdirectory);
  kitzoo::os::write_file(foreign, "unrelated");
  kitzoo::os::write_file(malformed, "unrelated");
  const auto old = std::filesystem::file_time_type::clock::now() - 2h;
  for (const auto& path : {closed, active, foreign, malformed})
    std::filesystem::last_write_time(path, old);
  EXPECT_EQ(sink->cleanup(), 1u);
  EXPECT_FALSE(std::filesystem::exists(closed));
  EXPECT_TRUE(std::filesystem::exists(active));
  EXPECT_TRUE(std::filesystem::exists(foreign));
  EXPECT_TRUE(std::filesystem::exists(malformed));
  EXPECT_TRUE(std::filesystem::exists(subdirectory));
  EXPECT_TRUE(sink->cleanup_error().empty());
}

TEST(LogRollingFileTest, CleanupDoesNotRemoveSymlinksOrTheirTargets) {
  TemporaryDirectory directory;
  Logger logger("links");
  auto sink = logger.add_file_sink({directory.path / "application.log", 64, 1s, 0ms});
  const auto target = directory.path / "unrelated.log";
  kitzoo::os::write_file(target, "keep");
  const auto link = directory.path / "application_20260101-000000_7.log";
  std::error_code error;
  std::filesystem::create_symlink(target, link, error);
  if (error)
    GTEST_SKIP() << "Symlink creation unavailable: " << error.message();
  std::filesystem::last_write_time(target, std::filesystem::file_time_type::clock::now() - 1h);
  EXPECT_EQ(sink->cleanup(), 0u);
  EXPECT_TRUE(std::filesystem::is_symlink(std::filesystem::symlink_status(link)));
  EXPECT_EQ(kitzoo::os::read_file(target), "keep");
}

TEST(LogRollingFileTest, BackgroundCleanupDeletesExpiredFilesWhileIdle) {
  TemporaryDirectory directory;
  Logger logger("idle");
  logger.set_pattern("%v");
  auto sink = logger.add_file_sink({directory.path / "application.log", 8, 1h, 10ms});
  logger.log(Level::Info, "first");
  const auto closed = sink->current_file();
  logger.log(Level::Info, "second");
  logger.flush();
  std::filesystem::last_write_time(closed, std::filesystem::file_time_type::clock::now() - 2h);
  const auto deadline = kitzoo::time::Deadline::after(5s);
  while (std::filesystem::exists(closed) && !deadline.expired())
    std::this_thread::sleep_for(1ms);
  EXPECT_FALSE(std::filesystem::exists(closed));
  EXPECT_TRUE(std::filesystem::exists(sink->current_file()));
  EXPECT_TRUE(sink->cleanup_error().empty());
}

TEST(LogRollingFileTest, DestructionInterruptsLongCleanupInterval) {
  TemporaryDirectory directory;
  const auto started = std::chrono::steady_clock::now();
  {
    RollingFileSink sink({directory.path / "application.log", 64, 0s, 1h});
  }
  EXPECT_LT(std::chrono::steady_clock::now() - started, 5s);
}

TEST(LogRollingFileTest, AsyncFlushPersistsEveryConcurrentProducerRecord) {
  TemporaryDirectory directory;
  LoggerOptions options;
  options.pattern = "%v";
  options.file = FileSinkOptions{directory.path / "application.log", 64, 0s, 0ms};
  auto logger = kitzoo::memory::make_shared<Logger>("async_file", options);
  AsyncLogger async(logger, {2});
  std::vector<std::jthread> producers;
  for (int worker = 0; worker < 4; ++worker)
    producers.emplace_back([&, worker]() -> void {
      for (int record = 0; record < 25; ++record)
        async.logf(Level::Info, std::source_location::current(), "{}:{}", worker, record);
    });
  producers.clear();
  async.flush();
  std::set<std::string> records;
  for (const auto& entry : std::filesystem::directory_iterator(directory.path)) {
    EXPECT_LE(entry.file_size(), 64u);
    std::istringstream text(kitzoo::os::read_file(entry.path()));
    for (std::string line; std::getline(text, line);) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      records.insert(line);
    }
  }
  ASSERT_EQ(records.size(), 100u);
  for (int worker = 0; worker < 4; ++worker)
    for (int record = 0; record < 25; ++record)
      EXPECT_TRUE(records.contains(std::to_string(worker) + ":" + std::to_string(record)));
  EXPECT_EQ(logger->failed_count(), 0u);
  EXPECT_EQ(async.failed_count(), 0u);
}

TEST(LogCallbackTest, CopiesPayloadBeforeCallbackReturns) {
  std::string copied;
  LoggerOptions options;
  options.sinks = {std::make_shared<CallbackSink>(
      [&](const auto& record) { copied.assign(record.payload.data(), record.payload.size()); })};
  Logger logger("callback", options);
  logger.logf(Level::Info, std::source_location::current(), "{}", std::string(1024, 'x'));
  EXPECT_EQ(copied, std::string(1024, 'x'));
}

TEST_F(LogTest, SinkErrorsInvokeHandler) {
  std::vector<std::string> errors;
  logger_->set_error_handler([&](std::string_view error) { errors.emplace_back(error); });
  logger_->add_sink(std::make_shared<CallbackSink>([](const auto&) { throw std::runtime_error("output failed"); }));
  EXPECT_NO_THROW(logger_->log(Level::Error, "attempt"));
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_NE(errors[0].find("output failed"), std::string::npos);
  EXPECT_EQ(logger_->failed_count(), 1u);
}

TEST_F(LogTest, ThrowingErrorHandlerTerminates) {
  logger_->set_error_handler([](std::string_view) { throw std::runtime_error("error callback failed"); });
  logger_->add_sink(std::make_shared<CallbackSink>([](const auto&) { throw std::runtime_error("output failed"); }));
  EXPECT_DEATH(logger_->log(Level::Error, "attempt"), "");
}

TEST_F(LogTest, AsyncFlushIsReusableAndFlushesPriorRecords) {
  AsyncLogger async(logger_, {2, OverflowPolicy::Block});
  async.logf(Level::Info, std::source_location::current(), "first={}", 1);
  async.flush();
  ASSERT_EQ(sink_->snapshot().size(), 1u);
  EXPECT_EQ(sink_->snapshot()[0].message, "first=1");
  EXPECT_EQ(sink_->flush_count.load(), 1);
  async.log(Level::Info, "second");
  async.flush();
  ASSERT_EQ(sink_->snapshot().size(), 2u);
  EXPECT_EQ(sink_->snapshot()[1].message, "second");
  EXPECT_EQ(sink_->flush_count.load(), 2);
}

TEST_F(LogTest, AsyncOwnsLongMessageAfterProducerExits) {
  AsyncLogger async(logger_, {2});
  std::jthread producer([&] {
    std::string text(4096, 'x');
    async.logf(Level::Info, std::source_location::current(), "{}:{}", text, 42);
    text.assign(4096, 'y');
  });
  producer.join();
  async.flush();
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].message, std::string(4096, 'x') + ":42");
}

TEST_F(LogTest, AsyncDestructorDrainsRecordsInOrder) {
  {
    AsyncLogger async(logger_, {2});
    for (int i = 0; i < 100; ++i)
      async.logf(Level::Info, std::source_location::current(), "{}", i);
  }
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 100u);
  for (std::size_t i = 0; i < records.size(); ++i)
    EXPECT_EQ(records[i].message, std::to_string(i));
  EXPECT_EQ(sink_->flush_count.load(), 1);
}

TEST_F(LogTest, AsyncBlockPolicyPreservesEveryConcurrentProducerMessage) {
  AsyncLogger async(logger_, {1, OverflowPolicy::Block});
  std::vector<std::jthread> producers;
  for (int t = 0; t < 4; ++t)
    producers.emplace_back([&, t] {
      for (int i = 0; i < 100; ++i)
        async.logf(Level::Info, std::source_location::current(), "{}:{}", t, i);
    });
  producers.clear();
  async.flush();
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 400u);
  std::set<std::string> messages;
  for (const auto& record : records)
    messages.insert(record.message);
  ASSERT_EQ(messages.size(), 400u);
  for (int t = 0; t < 4; ++t)
    for (int i = 0; i < 100; ++i)
      EXPECT_TRUE(messages.contains(std::to_string(t) + ":" + std::to_string(i)));
  EXPECT_EQ(async.dropped_count(), 0u);
}

TEST_F(LogTest, AsyncDropNewestPreservesAcceptedRecordsAndFlushBarrier) {
  OutputGate gate;
  logger_->add_sink(std::make_shared<CallbackSink>([&](const auto&) { gate.pause_first(); }));
  AsyncLogger async(logger_, {1, OverflowPolicy::DropNewest});
  async.log(Level::Info, "in flight");
  const bool entered = gate.wait_until_entered();
  if (!entered) {
    gate.release();
    FAIL() << "worker did not reach sink";
  }
  async.log(Level::Info, "queued");
  async.log(Level::Info, "discarded");
  EXPECT_EQ(async.dropped_count(), 1u);
  EXPECT_EQ(async.rejected_count(), 0u);
  gate.release();
  async.flush();
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 2u);
  EXPECT_EQ(records[0].message, "in flight");
  EXPECT_EQ(records[1].message, "queued");
  EXPECT_FALSE(gate.timed_out());
}

TEST_F(LogTest, AsyncCloseIsConcurrentAndRejectsFurtherSubmissions) {
  AsyncLogger async(logger_, {1});
  async.log(Level::Info, "before close");
  std::jthread first([&] { async.close(); });
  std::jthread second([&] { async.close(); });
  first.join();
  second.join();
  async.close();
  async.flush();
  async.log(Level::Info, "after close");
  EXPECT_EQ(sink_->snapshot().size(), 1u);
  EXPECT_EQ(async.rejected_count(), 1u);
  EXPECT_EQ(async.dropped_count(), 1u);
  EXPECT_EQ(sink_->flush_count.load(), 1);
}

TEST_F(LogTest, AsyncFailureDoesNotPreventOtherSinksOrLaterRecords) {
  std::atomic<int> errors{0};
  logger_->set_error_handler([&](std::string_view) { ++errors; });
  logger_->add_sink(std::make_shared<CallbackSink>([](const auto&) { throw std::runtime_error("output failed"); }));
  AsyncLogger async(logger_);
  async.log(Level::Info, "first");
  async.log(Level::Info, "second");
  async.flush();
  EXPECT_EQ(sink_->snapshot().size(), 2u);
  EXPECT_EQ(errors.load(), 2);
  EXPECT_EQ(logger_->failed_count(), 2u);
  EXPECT_EQ(async.failed_count(), 2u);
  EXPECT_EQ(async.dropped_count(), 0u);
}

TEST_F(LogTest, AsyncHonorsSinkFilteringAndAutomaticFlush) {
  sink_->set_level(spdlog::level::err);
  logger_->set_flush_level(Level::Error);
  AsyncLogger async(logger_);
  async.log(Level::Info, "sink filtered");
  async.log(Level::Error, "automatic flush");
  async.flush();
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].message, "automatic flush");
  EXPECT_EQ(sink_->flush_count.load(), 2);
}

TEST_F(LogTest, AsyncWorkerRejectsSelfWaitingFlushAndClose) {
  AsyncLogger* worker = nullptr;
  std::atomic<int> guarded{0};
  logger_->add_sink(std::make_shared<CallbackSink>([&](const auto&) {
    try {
      worker->flush();
    } catch (const std::logic_error&) {
      ++guarded;
    }
    try {
      worker->close();
    } catch (const std::logic_error&) {
      ++guarded;
    }
  }));
  AsyncLogger async(logger_);
  worker = &async;
  async.log(Level::Info, "callback");
  async.flush();
  EXPECT_EQ(guarded.load(), 2);
}

TEST_F(LogTest, AsyncRejectsMissingLoggerAndZeroCapacity) {
  EXPECT_THROW(AsyncLogger(nullptr), std::invalid_argument);
  EXPECT_THROW((AsyncLogger(logger_, {0})), std::invalid_argument);
}
} // namespace
