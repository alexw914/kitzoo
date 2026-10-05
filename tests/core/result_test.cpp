// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/result_test.cpp
// Description: Verifies Result value and error access, conversions and the
//              void specialization.
// -----------------------------------------------------------------------------

#include <kitzoo/core/result.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <system_error>
#include <variant>

using kitzoo::core::Result;
using kitzoo::core::Unexpected;

namespace {

auto parse_port(int value) -> Result<int, std::string> {
  if (value <= 0 || value > 65535)
    return Unexpected{std::string{"port out of range"}};
  return value;
}

auto open_device(bool ok) -> Result<void, std::error_code> {
  if (!ok)
    return Unexpected{std::make_error_code(std::errc::no_such_device)};
  return {};
}

} // namespace

TEST(ResultTest, HoldsValue) {
  auto result = parse_port(8080);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 8080);
  EXPECT_EQ(*result, 8080);
  EXPECT_EQ(result.value_or(1), 8080);
  EXPECT_THROW(static_cast<void>(result.error()), std::bad_variant_access);
}

TEST(ResultTest, HoldsError) {
  const auto result = parse_port(0);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), "port out of range");
  EXPECT_EQ(result.value_or(80), 80);
  EXPECT_THROW(static_cast<void>(result.value()), std::bad_variant_access);
}

TEST(ResultTest, SameValueAndErrorTypesStayDistinct) {
  Result<std::string, std::string> value{"ok"};
  Result<std::string, std::string> error{Unexpected{std::string{"bad"}}};
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value->size(), 2u);
  EXPECT_FALSE(error.has_value());
  EXPECT_EQ(error.error(), "bad");
}

TEST(ResultTest, MovesOnlyTypesOut) {
  Result<std::unique_ptr<int>, int> result{std::make_unique<int>(7)};
  auto owned = std::move(result).value();
  ASSERT_NE(owned, nullptr);
  EXPECT_EQ(*owned, 7);
}

TEST(ResultTest, VoidSpecializationReportsErrors) {
  const auto ok = open_device(true);
  EXPECT_TRUE(ok);
  EXPECT_NO_THROW(ok.value());
  const auto failed = open_device(false);
  ASSERT_FALSE(failed);
  EXPECT_EQ(failed.error(), std::errc::no_such_device);
  EXPECT_THROW(failed.value(), std::bad_variant_access);
}
