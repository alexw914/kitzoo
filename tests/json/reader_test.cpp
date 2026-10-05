// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/json/reader_test.cpp
// Description: Verifies Reader input selection, ownership, and load failures.
// -----------------------------------------------------------------------------

#include <kitzoo/json.hpp>
#include <kitzoo/log.hpp>

#include <cstdio>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

using namespace kitzoo::json;

namespace {

class ReaderFileTest : public testing::Test {
protected:
  auto SetUp() -> void override { dir_ = kitzoo::os::FsAdaptor::instance().temp_directory(); }

  auto TearDown() -> void override {
    std::error_code ec;
    kitzoo::os::FsAdaptor::instance().remove_all(dir_, ec);
  }

  std::filesystem::path dir_;
};

} // namespace

TEST(ReaderTest, AcceptsJsonTextOverloads) {
  const std::string text = R"({"workers":4})";
  Reader from_string(text);
  Reader from_view(std::string_view{text});
  Reader from_literal(R"({"workers":4})");
  for (const auto* reader : {&from_string, &from_view, &from_literal}) {
    ASSERT_TRUE(reader->is_parse_success()) << reader->error_info();
    EXPECT_EQ(reader->raw(), (Json{{"workers", 4}}));
    EXPECT_TRUE(reader->error_info().empty());
  }
}

TEST(ReaderTest, ErrorReferenceTracksLoadsAndCopies) {
  static_assert(std::is_same_v<decltype(std::declval<const Reader&>().error_info()), const kitzoo::memory::String&>);
  static_assert(noexcept(std::declval<const Reader&>().error_info()));
  Reader reader;
  const auto& error = reader.error_info();
  EXPECT_TRUE(error.empty());
  ASSERT_FALSE(reader.parse("{bad"));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(&error, &reader.error_info());
  Reader copy = reader;
  EXPECT_EQ(copy.error_info(), error);
  ASSERT_TRUE(reader.parse("{\"value\":1}"));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(reader.is_parse_success());
  EXPECT_FALSE(copy.is_parse_success());
  EXPECT_FALSE(copy.error_info().empty());
}

TEST(ReaderTest, ErrorInfoWorksWithLoggerAndPrintWithoutStringCopies) {
  Reader reader("{bad");
  ASSERT_FALSE(reader.is_parse_success());
  std::string captured;
  kitzoo::log::LoggerOptions options;
  options.sinks.push_back(
      kitzoo::memory::make_shared<kitzoo::log::CallbackSink>([&](const spdlog::details::log_msg& message) -> void {
        captured.assign(message.payload.data(), message.payload.size());
      }));
  kitzoo::log::Logger logger("reader", options);
  logger.log(kitzoo::log::Level::Error, reader.error_info());
  EXPECT_EQ(captured, "Invalid JSON text");
  logger.logf(kitzoo::log::Level::Error, std::source_location::current(), "JSON failed: {}", reader.error_info());
  EXPECT_EQ(captured, "JSON failed: Invalid JSON text");

  auto close_file = [](std::FILE* file) noexcept -> void { (void)std::fclose(file); };
  kitzoo::memory::UniquePtr<std::FILE, decltype(close_file)> output{std::tmpfile(), close_file};
  ASSERT_TRUE(output);
  fmt::print(output.get(), "{}", reader.error_info());
  std::rewind(output.get());
  char text[64]{};
  const auto size = std::fread(text, 1, sizeof(text), output.get());
  EXPECT_EQ(std::string_view(text, size), "Invalid JSON text");
}

TEST(ReaderTest, OwnsCopiedAndMovedJsonValues) {
  Json original{{"workers", 4}};
  Reader copy(original);
  original["workers"] = 8;
  EXPECT_EQ(copy.raw().at("workers"), 4);
  Reader moved(std::move(original));
  EXPECT_EQ(moved.raw().at("workers"), 8);
  moved.raw()["workers"] = 9;
  const Reader& read_only = moved;
  EXPECT_EQ(read_only.raw().at("workers"), 9);
  Reader null_value(Json(nullptr));
  EXPECT_TRUE(null_value.is_parse_success());
  EXPECT_TRUE(null_value.raw().is_null());
  Json discarded = Json::parse("{bad", nullptr, false);
  Reader invalid(std::move(discarded));
  EXPECT_FALSE(invalid.is_parse_success());
  EXPECT_FALSE(invalid.error_info().empty());
}

TEST(ReaderTest, FailedParsePreservesDocumentAndSuccessfulParseClearsError) {
  Reader reader;
  EXPECT_TRUE(reader.raw().is_object());
  ASSERT_TRUE(reader.parse("[1,2,3]"));
  for (auto text : {"", "{bad", "{\"x\":1} trailing", "1e9999"}) {
    EXPECT_FALSE(reader.parse(text));
    EXPECT_FALSE(reader.is_parse_success());
    EXPECT_FALSE(reader.error_info().empty());
    EXPECT_EQ(reader.raw(), Json::array({1, 2, 3}));
  }
  ASSERT_TRUE(reader.parse("null"));
  EXPECT_TRUE(reader.is_parse_success());
  EXPECT_TRUE(reader.error_info().empty());
  EXPECT_TRUE(reader.raw().is_null());
}

TEST_F(ReaderFileTest, ReadsFilesystemPathAndStringPath) {
  const auto path = dir_ / "config.json";
  kitzoo::os::FsAdaptor::instance().write_text(path, R"({"workers":4})");
  Reader from_path(path);
  ASSERT_TRUE(from_path.is_parse_success()) << from_path.error_info();
  EXPECT_EQ(from_path.raw(), (Json{{"workers", 4}}));
  Reader from_string_path;
  ASSERT_TRUE(from_string_path.load_file(path.string()));
  EXPECT_EQ(from_string_path.raw(), from_path.raw());
  // A string constructor always parses text; it never guesses that the string is a file path.
  Reader path_as_text(path.string());
  EXPECT_FALSE(path_as_text.is_parse_success());
}

TEST_F(ReaderFileTest, FileErrorsPreserveDocumentAndAllowRecovery) {
  const auto path = dir_ / "config.json";
  Reader reader(Json{{"workers", 4}});
  EXPECT_FALSE(reader.load_file(path));
  EXPECT_FALSE(reader.is_parse_success());
  EXPECT_FALSE(reader.error_info().empty());
  EXPECT_EQ(reader.raw(), (Json{{"workers", 4}}));
  kitzoo::os::FsAdaptor::instance().write_text(path, "{bad");
  EXPECT_FALSE(reader.load_file(path));
  EXPECT_EQ(reader.raw(), (Json{{"workers", 4}}));
  kitzoo::os::FsAdaptor::instance().write_text(path, R"({"workers":8})");
  ASSERT_TRUE(reader.load_file(path));
  EXPECT_TRUE(reader.is_parse_success());
  EXPECT_TRUE(reader.error_info().empty());
  EXPECT_EQ(reader.raw(), (Json{{"workers", 8}}));
  Reader missing(dir_ / "missing.json");
  EXPECT_FALSE(missing.is_parse_success());
}
