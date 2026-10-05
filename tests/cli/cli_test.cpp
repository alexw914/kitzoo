#include <kitzoo/cli.hpp>

#include <gtest/gtest.h>
#include <string>

TEST(CliTest, ParsesTypedOptionsAndDefaults) {
  kitzoo::cli::Options options{"test", "CLI test"};
  options.add_options()("count", "Number of items",
                        kitzoo::cli::value<int>()->default_value("1"))("v,verbose", "Enable verbose output");

  char const* argv[]{"test", "--count", "5", "-v"};
  auto const result = options.parse(4, argv);

  EXPECT_EQ(result["count"].as<int>(), 5);
  EXPECT_TRUE(result["verbose"].as<bool>());
}
