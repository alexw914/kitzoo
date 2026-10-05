// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/log/file_sink_test.cpp
// Description: Verifies rolling file naming, size rollover, age-based cleanup and
//              asynchronous delivery to files.
// -----------------------------------------------------------------------------

#include <kitzoo/log.hpp>
#include <kitzoo/os/filesys.hpp>
#include <kitzoo/time/time.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <set>
#include <spdlog/details/os.h>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace kitzoo::log;
using namespace std::chrono_literals;

class TemporaryDirectory {
public:
  TemporaryDirectory() : path(kitzoo::os::temp_directory()) {}

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  std::filesystem::path path;
};

TEST(FileSinkTest, RejectsInvalidOptions) {
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

TEST(FileSinkTest, OptionsCreateDirectoriesAndRotateWithoutRenamingOldFiles) {
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

TEST(FileSinkTest, OversizedRecordStaysIntactAndNextRecordStartsNewFile) {
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

TEST(FileSinkTest, ExactSizeBoundaryDoesNotSplitTheRecord) {
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

TEST(FileSinkTest, NamesFilesWithTimestampAndKeepsHistoryAcrossRestart) {
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

TEST(FileSinkTest, AgeCleanupPreservesActiveAndUnrelatedFiles) {
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

TEST(FileSinkTest, CleanupDoesNotRemoveSymlinksOrTheirTargets) {
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

TEST(FileSinkTest, BackgroundCleanupDeletesExpiredFilesWhileIdle) {
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

TEST(FileSinkTest, DestructionInterruptsLongCleanupInterval) {
  TemporaryDirectory directory;
  const auto started = std::chrono::steady_clock::now();
  {
    RollingFileSink sink({directory.path / "application.log", 64, 0s, 1h});
  }
  EXPECT_LT(std::chrono::steady_clock::now() - started, 5s);
}

TEST(FileSinkTest, AsyncFlushPersistsEveryConcurrentProducerRecord) {
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

} // namespace
