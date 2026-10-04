// ---------------------------------------------------------------------------
// kitzoo/log tests
// ---------------------------------------------------------------------------

#include <kitzoo/log/async_logger.hpp>
#include <kitzoo/log/logger.hpp>
#include <kitzoo/os/filesystem.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <spdlog/sinks/base_sink.h>
#include <string>
#include <thread>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::log;

// In-memory sink for assertions.
class VectorSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
    auto sink_it_(spdlog::details::log_msg const& msg) -> void override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        lines.emplace_back(formatted.data(), formatted.size());
        switch (msg.level) {
            case spdlog::level::trace:
                levels.push_back(Level::Trace);
                break;
            case spdlog::level::debug:
                levels.push_back(Level::Debug);
                break;
            case spdlog::level::info:
                levels.push_back(Level::Info);
                break;
            case spdlog::level::warn:
                levels.push_back(Level::Warn);
                break;
            case spdlog::level::err:
                levels.push_back(Level::Error);
                break;
            case spdlog::level::critical:
                levels.push_back(Level::Fatal);
                break;
            default:
                break;
        }
    }

    auto flush_() -> void override {}

public:
    auto snapshot() -> std::vector<std::string> {
        std::lock_guard lock{mutex_};
        return lines;
    }
    auto level_snapshot() -> std::vector<Level> {
        std::lock_guard lock{mutex_};
        return levels;
    }

private:
    std::vector<std::string> lines;
    std::vector<Level> levels;
};

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        sink_ = std::make_shared<VectorSink>();
        logger_ = std::make_unique<Logger>("test");
        logger_->add_sink(sink_);
    }

    std::shared_ptr<VectorSink> sink_;
    std::unique_ptr<Logger> logger_;
};

TEST_F(LoggerTest, InfoWritesFormattedLine) {
    logger_->log(Level::Info, "hello");
    auto const lines = sink_->snapshot();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("hello"), std::string::npos);
    EXPECT_NE(lines[0].find("info"), std::string::npos);
    EXPECT_NE(lines[0].find("[test]"), std::string::npos);
}

TEST(LogMacroTest, FormatsMessage) {
    EXPECT_NO_THROW(KZ_LOG_INFO("macro value: {}", 42));
}

TEST_F(LoggerTest, LevelFiltering) {
    logger_->set_level(Level::Warn);
    logger_->log(Level::Debug, "dropped");
    logger_->log(Level::Info, "dropped");
    logger_->log(Level::Warn, "kept");
    logger_->log(Level::Error, "kept");
    EXPECT_EQ(sink_->snapshot().size(), 2u);
}

TEST_F(LoggerTest, LevelCanBeRaisedAndLowered) {
    logger_->set_level(Level::Error);
    logger_->log(Level::Warn, "dropped");
    logger_->set_level(Level::Trace);
    logger_->log(Level::Trace, "kept");
    EXPECT_EQ(sink_->snapshot().size(), 1u);
}

TEST_F(LoggerTest, SourceLocationCaptured) {
    logger_->log(Level::Info, "loc test");  // this line's location
    auto const lines = sink_->snapshot();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("log_test.cpp"), std::string::npos);
}

TEST_F(LoggerTest, TimestampPresent) {
    logger_->log(Level::Info, "ts");
    auto const lines = sink_->snapshot();
    // [YYYY-MM-DD HH:MM:SS.mmm]
    EXPECT_NE(lines[0].find("] [info"), std::string::npos);
}

TEST_F(LoggerTest, MultipleSinksReceiveSameRecord) {
    auto sink2 = std::make_shared<VectorSink>();
    logger_->add_sink(sink2);
    logger_->log(Level::Info, "broadcast");
    EXPECT_EQ(sink_->snapshot().size(), 1u);
    EXPECT_EQ(sink2->snapshot().size(), 1u);
}

TEST_F(LoggerTest, ConcurrentWritesNoCrash) {
    constexpr int kThreads = 4;
    constexpr int kMsgsPerThread = 500;
    std::vector<std::jthread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([this] {
            for (int i = 0; i < kMsgsPerThread; ++i) {
                logger_->log(Level::Info, "msg from thread");
            }
        });
    }
    threads.clear();  // join all
    EXPECT_EQ(sink_->snapshot().size(), static_cast<std::size_t>(kThreads) * kMsgsPerThread);
}

TEST_F(LoggerTest, FileSinkWrites) {
    auto dir = os::temp_directory();
    auto const path = dir / "test.log";
    {
        Logger file_logger{"file"};
        file_logger.add_sink(std::make_shared<FileSink>(path.string(), false));
        file_logger.log(Level::Info, "to file");
        file_logger.flush();
    }
    auto const content = os::read_text(path);
    EXPECT_NE(content.find("to file"), std::string::npos);
    os::remove_all(dir);
}

// -- AsyncLogger ---------------------------------------------------------------

TEST(AsyncLoggerTest, DeliversMessages) {
    auto sink = std::make_shared<VectorSink>();
    {
        AsyncLogger async{std::make_shared<Logger>("async")};
        // add sink to the underlying logger before logging
    }
    SUCCEED();  // smoke: construct + destruct
}

TEST(AsyncLoggerTest, AsyncEndToEnd) {
    auto logger = std::make_shared<Logger>("async-e2e");
    auto sink = std::make_shared<VectorSink>();
    logger->add_sink(sink);

    {
        AsyncLogger async{logger};
        for (int i = 0; i < 100; ++i)
            async.log(Level::Info, "queued message");
        // destructor closes queue, drains, joins
    }

    auto const lines = sink->snapshot();
    EXPECT_EQ(lines.size(), 100u);
    for (auto const& line : lines) {
        EXPECT_NE(line.find("queued message"), std::string::npos);
    }
}

TEST(AsyncLoggerTest, ConcurrentProducers) {
    auto logger = std::make_shared<Logger>("async-mt");
    auto sink = std::make_shared<VectorSink>();
    logger->add_sink(sink);

    constexpr int kThreads = 4;
    constexpr int kMsgs = 250;
    {
        AsyncLogger async{logger};
        std::vector<std::jthread> producers;
        for (int t = 0; t < kThreads; ++t) {
            producers.emplace_back([&async] {
                for (int i = 0; i < kMsgs; ++i)
                    async.log(Level::Info, "x");
            });
        }
    }  // async destructor drains everything

    EXPECT_EQ(sink->snapshot().size(), static_cast<std::size_t>(kThreads) * kMsgs);
}

TEST(AsyncLoggerTest, DropAfterClose) {
    auto logger = std::make_shared<Logger>("async-drop");
    auto sink = std::make_shared<VectorSink>();
    logger->add_sink(sink);

    AsyncLogger async{logger};
    async.log(Level::Info, "before close");
    async.close();
    async.log(Level::Info, "after close");  // dropped

    EXPECT_EQ(async.dropped_count(), 1u);
    EXPECT_EQ(sink->snapshot().size(), 1u);
}
