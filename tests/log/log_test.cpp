// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/log/log_test.cpp
// Description: Verifies logging records, configuration, bounded delivery and errors.
// -----------------------------------------------------------------------------

#include <kitzoo/log.hpp>
#include <kitzoo/os/filesys.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <set>
#include <spdlog/details/os.h>
#include <spdlog/sinks/base_sink.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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

const std::string kEol{spdlog::details::os::default_eol};

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

auto log_from_named_function(Logger& logger) -> void {
  logger.log(Level::Info, "free");
}

struct Camera {
  auto capture(Logger& logger) const -> int {
    logger.log(Level::Info, "member");
    return 0;
  }
};

template <typename T>
auto log_from_template(Logger& logger, std::vector<T>) -> void {
  logger.log(Level::Info, "template");
}

TEST_F(LogTest, FunctionFlagPrintsCallerNameAndLambdasTheirEnclosingFunction) {
  logger_->set_pattern("%*");
  log_from_named_function(*logger_);
  (void)Camera{}.capture(*logger_);
  log_from_template(*logger_, std::vector<int>{});
  [this] { logger_->log(Level::Info, "lambda"); }();
  [this] { [this] { logger_->log(Level::Info, "nested"); }(); }();
  [this](auto value) {
    logger_->log(Level::Info, "generic");
    return value;
  }(1);
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 6u);
  EXPECT_EQ(records[0].formatted, "log_from_named_function" + kEol);
  EXPECT_EQ(records[1].formatted, "capture" + kEol);
  EXPECT_EQ(records[2].formatted, "log_from_template" + kEol);
  for (std::size_t i = 3; i < records.size(); ++i)
    EXPECT_EQ(records[i].formatted, "TestBody" + kEol) << i;
}

// Signatures as clang, GCC and MSVC spell them in std::source_location.
TEST_F(LogTest, FunctionFlagParsesEachCompilersSignatures) {
  logger_->set_pattern("%*");
  const std::vector<std::pair<const char*, const char*>> cases{
      {"int main()", "main"},
      {"int app::Channel::run(int) const", "run"},
      {"void (anonymous namespace)::file_logging()", "file_logging"},
      {"void process(std::vector<T>) [T = int]", "process"},
      {"std::vector<int> make()", "make"},
      {"int Functor::operator()(int)", "operator()"},
      {"auto outer()::(anonymous class)::operator()() const", "outer"},
      {"auto app::Channel::run()::(anonymous class)::operator()() const", "run"},
      {"auto outer()::(anonymous class)::operator()()::(anonymous class)::operator()() const", "outer"},
      {"auto outer()::(anonymous class)::operator()(auto) const [x:auto = int]", "outer"},
      {"auto app::Channel::(anonymous class)::operator()() const", "lambda"},
      {"auto (anonymous class)::operator()() const", "lambda"},
      {"auto (anonymous namespace)::(anonymous class)::operator()() const", "lambda"},
      {"void {anonymous}::file_logging()", "file_logging"},
      {"void process(std::vector<T>) [with T = int]", "process"},
      {"{anonymous}::asynchronous_logging()::<lambda()>", "asynchronous_logging"},
      {"void app::Channel::run()::<lambda()>", "run"},
      {"outer()::<lambda()>::<lambda()>", "outer"},
      {"outer()::<lambda(auto:1)> [with auto:1 = int]", "outer"},
      {"app::Channel::<lambda()>", "lambda"},
      {"{anonymous}::<lambda()>", "lambda"},
      {"<lambda()>", "lambda"},
      {"int __cdecl main(void)", "main"},
      {"int __thiscall app::Channel::run(int) const", "run"},
      {"void __cdecl `anonymous-namespace'::file_logging(void)", "file_logging"},
      {"auto __cdecl outer::<lambda_1>::operator ()(void) const", "outer"},
      {"auto __cdecl `anonymous-namespace'::asynchronous_logging::<lambda_1>::operator ()(void) const",
       "asynchronous_logging"},
      {"auto __cdecl `anonymous-namespace'::<lambda_1>::operator ()(void) const", "lambda"},
      {"auto __cdecl <lambda_1>::operator ()(void) const", "lambda"},
      {"void __cdecl `anonymous-namespace'::log_from_template<int>(class std::vector<int,class std::allocator<int> >)",
       "log_from_template"},
      {"auto __cdecl S::TestBody::<lambda_2>::()::<lambda_1>::operator ()(void) const", "TestBody"},
      {"auto __cdecl S::TestBody::<lambda_3>::operator ()<int>(int) const", "TestBody"},
      {"bool __cdecl operator<=>(const S &,const S &)", "operator<=>"},
  };
  for (const auto& [signature, expected] : cases) {
    sink_->log(
        spdlog::details::log_msg{spdlog::source_loc{"file.cpp", 1, signature}, "test", spdlog::level::info, "x"});
    EXPECT_EQ(sink_->snapshot().back().formatted, expected + kEol) << signature;
  }
}

TEST(LogFormatTest, DefaultPatternShowsThreadLevelLetterAndCaller) {
  auto sink = std::make_shared<RecordingSink>();
  LoggerOptions options;
  options.sinks = {sink};
  Logger logger("format", options);
  log_from_named_function(logger);
  const auto formatted = sink->snapshot().at(0).formatted;
  // [YYYY-mm-dd HH:MM:SS.mmm][<tid>][I][log_test.cpp:<line>,log_from_named_function] free
  EXPECT_EQ(formatted[0], '[');
  EXPECT_EQ(formatted[24], ']');
  EXPECT_NE(formatted.find("][I][log_test.cpp:"), std::string::npos) << formatted;
  EXPECT_NE(formatted.find(",log_from_named_function] free"), std::string::npos) << formatted;
  EXPECT_EQ(formatted.find("[format]"), std::string::npos) << formatted;
}

// Callers need only the header: the macros qualify every name, so a caller's
// own std, log or default_logger cannot capture them.
namespace macro_caller {
namespace std {
struct source_location {};
} // namespace std

namespace log {} // namespace log

inline auto default_logger() -> int {
  return 0;
}

inline auto emit(int frame) -> void {
  KZ_LOG_DEBUG("shadowed frame={}", frame);
  KZ_CHECK(frame >= 0);
}
} // namespace macro_caller

TEST(LogMacroTest, MacrosIgnoreShadowingNamesInCallerNamespace) {
  auto& logger = default_logger();
  logger.set_level(Level::Off);
  macro_caller::emit(1);
  logger.set_level(Level::Info);
  EXPECT_EQ(macro_caller::default_logger(), 0);
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

auto checked_half(int value) -> int {
  KZ_CHECK_RETURN(value % 2 == 0, -1, "odd value {}", value);
  return value / 2;
}

auto checked_count(int value, int& calls) -> void {
  KZ_CHECK_RETURN(value > 0, , "non-positive value {}", value);
  ++calls;
}

TEST(LogMacroTest, CheckReturnReturnsInsteadOfAborting) {
  auto& logger = default_logger();
  logger.set_level(Level::Off);
  EXPECT_EQ(checked_half(4), 2);
  EXPECT_EQ(checked_half(3), -1);
  int calls = 0;
  checked_count(1, calls);
  checked_count(0, calls);
  logger.set_level(Level::Info);
  EXPECT_EQ(calls, 1);
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
  EXPECT_EQ(async.dropped_count(), 0u);
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

TEST_F(LogTest, AsyncRecordsPrintTheSameThreadIdAsSynchronousOnes) {
  logger_->set_pattern("%t");
  logger_->log(Level::Info, "sync");
  AsyncLogger async(logger_);
  async.log(Level::Info, "async");
  async.flush();
  const auto records = sink_->snapshot();
  ASSERT_EQ(records.size(), 2u);
  EXPECT_EQ(records[0].formatted, records[1].formatted);
}

TEST_F(LogTest, AsyncRejectsMissingLoggerAndZeroCapacity) {
  EXPECT_THROW(AsyncLogger(nullptr), std::invalid_argument);
  EXPECT_THROW((AsyncLogger(logger_, {0})), std::invalid_argument);
}

// init() reconfigures the process-wide default logger, so each case runs in a
// child process.
auto log_files(const std::filesystem::path& directory) -> std::vector<std::filesystem::path> {
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator{directory})
    if (entry.path().filename().string().starts_with("app_"))
      files.push_back(entry.path());
  return files;
}

auto read_all(const std::vector<std::filesystem::path>& files) -> std::string {
  std::string text;
  for (const auto& file : files)
    text += kitzoo::os::read_file(file);
  return text;
}

auto verify_init_routes_macros_to_rolling_files() -> void {
  const auto directory = kitzoo::os::temp_directory();
  const auto expired = directory / "app_20260101-000000_0.log";
  kitzoo::os::write_file(expired, "expired");
  std::filesystem::last_write_time(expired, std::filesystem::file_time_type::clock::now() - 2h);

  auto sink =
      init({.level = Level::Debug,
            .pattern = "%v",
            .console = false,
            .file = {.path = directory / "app.log", .max_size_bytes = 64, .max_age = 1h, .cleanup_interval = 0ms}});
  EXPECT_NE(sink, nullptr);
  for (int frame = 0; frame < 20; ++frame)
    KZ_LOG_DEBUG("frame={}", frame);
  default_logger().flush();

  EXPECT_FALSE(std::filesystem::exists(expired));
  const auto files = log_files(directory);
  EXPECT_GT(files.size(), 1u);
  const auto text = read_all(files);
  for (int frame = 0; frame < 20; ++frame)
    EXPECT_NE(text.find(fmt::format("frame={}{}", frame, kEol)), std::string::npos) << frame;
  std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

auto verify_init_replaces_outputs() -> void {
  const auto directory = kitzoo::os::temp_directory();
  auto sink =
      init({.pattern = "%v", .console = false, .file = {.path = directory / "app.log", .cleanup_interval = 0ms}});
  KZ_LOG_INFO("to file");
  default_logger().flush();
  EXPECT_EQ(init({.console = false}), nullptr);
  KZ_LOG_INFO("dropped");
  default_logger().flush();
  EXPECT_EQ(kitzoo::os::read_file(sink->current_file()), "to file" + kEol);
  std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

auto verify_failed_init_keeps_configuration() -> void {
  const auto directory = kitzoo::os::temp_directory();
  auto sink =
      init({.pattern = "%v", .console = false, .file = {.path = directory / "app.log", .cleanup_interval = 0ms}});
  EXPECT_THROW(init({.console = false, .file = {.path = directory / ".."}}), std::invalid_argument);
  KZ_LOG_INFO("still logged");
  default_logger().flush();
  EXPECT_EQ(kitzoo::os::read_file(sink->current_file()), "still logged" + kEol);
  std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

TEST(LogInitDeathTest, RoutesMacrosToRollingFilesAndCleansExpiredFiles) {
  EXPECT_EXIT(verify_init_routes_macros_to_rolling_files(), ::testing::ExitedWithCode(0), "");
}

TEST(LogInitDeathTest, ReplacesOutputsOnEachCall) {
  EXPECT_EXIT(verify_init_replaces_outputs(), ::testing::ExitedWithCode(0), "");
}

TEST(LogInitDeathTest, FailedInitKeepsConfiguration) {
  EXPECT_EXIT(verify_failed_init_keeps_configuration(), ::testing::ExitedWithCode(0), "");
}
} // namespace
