// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/cli_test.cpp
// Description: Verifies typed command-line options, defaults, help, and parse errors.
// -----------------------------------------------------------------------------

#include <kitzoo/core.hpp>

#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(CliTest, ParsesTypedOptionsAndDefaults) {
  kitzoo::core::Options options{"test", "CLI test"};
  options.add_options()("c,count", "Number of items", kitzoo::core::value<int>())(
      "name", "Name", kitzoo::core::value<std::string>()->default_value("world"))(
      "ids", "Item IDs", kitzoo::core::value<std::vector<int>>())("v,verbose", "Verbose output",
                                                                  kitzoo::core::value<bool>()->default_value("false"));

  const char* argv[]{"test", "--count", "5", "--ids", "1,2", "-v"};
  const auto result = options.parse(6, argv);

  EXPECT_EQ(result["count"].as<int>(), 5);
  EXPECT_EQ(result["name"].as<std::string>(), "world");
  EXPECT_EQ(result["ids"].as<std::vector<int>>(), (std::vector<int>{1, 2}));
  EXPECT_TRUE(result["verbose"].as<bool>());
  EXPECT_GT(result.count("count"), 0U);
  EXPECT_EQ(result.count("name"), 0U);
}

TEST(CliTest, ProvidesHelpAndDefaultFlag) {
  kitzoo::core::Options options{"test", "CLI test"};
  options.add_options()("h,help", "Show help", kitzoo::core::value<bool>()->default_value("false"));

  const char* argv[]{"test"};
  const auto result = options.parse(1, argv);

  EXPECT_FALSE(result["help"].as<bool>());
  EXPECT_EQ(result.count("help"), 0U);
  EXPECT_NE(options.help().find("--help"), std::string::npos);
}

TEST(CliTest, RejectsInvalidValues) {
  kitzoo::core::Options options{"test", "CLI test"};
  options.add_options()("count", "Number of items", kitzoo::core::value<int>());
  const char* argv[]{"test", "--count", "invalid"};

  EXPECT_THROW(static_cast<void>(options.parse(3, argv)), cxxopts::exceptions::exception);
}

TEST(CliTest, AppliesDefaultsAndParsesVectorValues) {
  kitzoo::core::Options options{"test", "CLI value test"};
  using kitzoo::core::value;
  options.add_options()("count", "Number of items", value<int>())(
      "name", "Name", value<std::string>()->default_value("world"))("ids", "Item IDs", value<std::vector<int>>())(
      "fallback", "Fallback IDs", value<std::vector<int>>()->default_value("3,4"));

  const char* argv[]{"test", "--count", "5", "--ids", "1,2"};
  const auto result = options.parse(5, argv);

  EXPECT_EQ(result["count"].as<int>(), 5);
  EXPECT_EQ(result["name"].as<std::string>(), "world");
  EXPECT_EQ(result["ids"].as<std::vector<int>>(), (std::vector<int>{1, 2}));
  EXPECT_EQ(result["fallback"].as<std::vector<int>>(), (std::vector<int>{3, 4}));
  EXPECT_EQ(result.count("name"), 0U);
}
