// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/version_test.cpp
// Description: Verifies version constants and build metadata.
// -----------------------------------------------------------------------------

#include <kitzoo/core/version.hpp>

#include <gtest/gtest.h>
#include <string_view>

using namespace kitzoo::core;

TEST(VersionTest, LibraryVersionIsNotAllZero) {
  auto v = library_version;
  EXPECT_TRUE(v.major != 0u || v.minor != 0u || v.patch != 0u);
}

TEST(VersionTest, VersionStringIsNotEmpty) {
  EXPECT_FALSE(version_string.empty());
}

TEST(VersionTest, VersionStringMatchesStruct) {
  auto major = std::to_string(library_version.major);
  EXPECT_NE(version_string.find(major), std::string_view::npos);
}

TEST(VersionTest, ThreeWayComparison) {
  version const v1{1, 0, 0};
  version const v2{1, 0, 1};
  version const v3{2, 0, 0};
  version const v4{1, 0, 0};

  EXPECT_LT(v1, v2);
  EXPECT_LT(v2, v3);
  EXPECT_EQ(v1, v4);
  EXPECT_NE(v1, v2);
  EXPECT_LE(v1, v4);
  EXPECT_GE(v3, v2);
}

TEST(VersionBuildInfoTest, CurrentBuildInfoReturnsValid) {
  build_info const& info = current_build_info();
  EXPECT_EQ(info.lib_version, library_version);
  EXPECT_EQ(info.version_str, version_string);
  EXPECT_FALSE(info.compiler.empty());
  EXPECT_FALSE(info.sanitizers.empty());
}

TEST(VersionBuildInfoTest, IsDebugMatchesContext) {
  build_info const& info = current_build_info();
#ifdef NDEBUG
  EXPECT_FALSE(info.is_debug);
#else
  EXPECT_TRUE(info.is_debug);
#endif
}
