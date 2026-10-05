// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/os/fs_test.cpp
// Description: Verifies filesystem operations and error handling.
// -----------------------------------------------------------------------------

#include <kitzoo/os.hpp>

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
class FsTest : public ::testing::Test {
protected:
  auto SetUp() -> void override { root_ = kitzoo::os::temp_directory(); }

  auto TearDown() -> void override {
    std::error_code ec;
    kitzoo::os::remove_path(root_, ec, true);
  }

  auto path(std::string_view name) const -> std::filesystem::path { return root_ / name; }

  std::filesystem::path root_;
};

TEST_F(FsTest, PreservesBinaryAndText) {
  const std::string bytes("a\0b", 3);
  kitzoo::os::write_file(path("binary"), bytes);
  EXPECT_EQ(kitzoo::os::read_file(path("binary")), bytes);
  kitzoo::os::write_file(path("text"), "old");
  EXPECT_EQ(kitzoo::os::read_file(path("text")), "old");
  const std::string updated = "new";
  kitzoo::os::atomic_write(path("text"), updated);
  EXPECT_EQ(kitzoo::os::read_file(path("text")), updated);
}

TEST_F(FsTest, ReadsEmptyAndLargeFilesAndReportsMissingFiles) {
  kitzoo::os::write_file(path("empty"), std::string_view{});
  EXPECT_TRUE(kitzoo::os::read_file(path("empty")).empty());
  const std::string data(10'000, '\xAB');
  kitzoo::os::write_file(path("large"), data);
  EXPECT_EQ(kitzoo::os::read_file(path("large")), data);

  std::error_code ec;
  EXPECT_TRUE(kitzoo::os::read_file(path("missing"), ec).empty());
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
  EXPECT_THROW(static_cast<void>(kitzoo::os::read_file(path("missing"))), std::filesystem::filesystem_error);
}

TEST_F(FsTest, ReadReportsPermissionDenied) {
  kitzoo::os::write_file(path("locked"), "secret");
  std::filesystem::permissions(path("locked"), std::filesystem::perms::none);
  std::error_code ec;
  const auto content = kitzoo::os::read_file(path("locked"), ec);
  std::filesystem::permissions(path("locked"), std::filesystem::perms::owner_all);
  if (!ec)
    GTEST_SKIP() << "permissions are not enforced for this user";
  EXPECT_TRUE(content.empty());
  EXPECT_EQ(ec, std::errc::permission_denied);
}

TEST_F(FsTest, AtomicWriteCreatesParentsAndPreservesFileOnFailure) {
  const std::string data = "content";
  kitzoo::os::atomic_write(path("nested/deep/file"), data);
  EXPECT_EQ(kitzoo::os::read_file(path("nested/deep/file")), data);
  kitzoo::os::write_file(path("parent"), "existing file");

  std::error_code ec;
  kitzoo::os::atomic_write(path("parent/file"), std::string_view{}, ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(kitzoo::os::read_file(path("parent")), "existing file");
  EXPECT_EQ(kitzoo::os::list_directory(root_).size(), 2u);
}

TEST_F(FsTest, TempDirectoriesAreUnique) {
  const auto directory = kitzoo::os::temp_directory();
  EXPECT_NE(directory, root_);
  EXPECT_TRUE(kitzoo::os::is_dir(directory));
  kitzoo::os::remove_path(directory);
}

TEST_F(FsTest, UnicodePathsSupportAtomicReplacement) {
  const auto filename = root_ / std::filesystem::path(u8"\u6d4b\u8bd5/\u6570\u636e.txt");
  const std::string initial = "initial";
  kitzoo::os::atomic_write(filename, initial);
  const std::string updated = "updated";
  kitzoo::os::atomic_write(filename, updated);
  kitzoo::os::append_file(filename, "!");
  EXPECT_EQ(kitzoo::os::read_file(filename), "updated!");
  EXPECT_EQ(kitzoo::os::list_directory(filename.parent_path()).size(), 1u);
}

TEST_F(FsTest, MakeDirectoryCreatesOnlyRequestedLevelUnlessParentsEnabled) {
  std::error_code ec = std::make_error_code(std::errc::io_error);
  EXPECT_TRUE(kitzoo::os::make_directory(path("one"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(kitzoo::os::make_directory(path("one")));
  EXPECT_FALSE(kitzoo::os::make_directory(path("missing/deep"), ec));
  EXPECT_TRUE(ec);
  EXPECT_TRUE(kitzoo::os::make_directory(path("missing/deep"), ec, true));
  EXPECT_FALSE(ec);
  EXPECT_TRUE(kitzoo::os::is_dir(path("missing/deep")));
  EXPECT_FALSE(kitzoo::os::make_directory(path("missing/deep"), true));
}

TEST_F(FsTest, MakeDirectoryRejectsExistingFile) {
  kitzoo::os::write_file(path("file"), "content");
  std::error_code ec;
  EXPECT_FALSE(kitzoo::os::make_directory(path("file"), ec));
  EXPECT_TRUE(ec);
  EXPECT_THROW(kitzoo::os::make_directory(path("file")), std::filesystem::filesystem_error);
}

TEST_F(FsTest, PredicatesDistinguishFilesFoldersAndMissingPaths) {
  kitzoo::os::write_file(path("empty"), "");
  kitzoo::os::make_directory(path("folder"));
  EXPECT_TRUE(kitzoo::os::path_exists(path("empty")));
  EXPECT_TRUE(kitzoo::os::is_regular(path("empty")));
  EXPECT_FALSE(kitzoo::os::is_dir(path("empty")));
  EXPECT_TRUE(kitzoo::os::is_dir(path("folder")));
  EXPECT_FALSE(kitzoo::os::is_regular(path("folder")));
  kitzoo::os::write_file(path("folder/data"), "x");
  std::error_code ec = std::make_error_code(std::errc::io_error);
  EXPECT_FALSE(kitzoo::os::path_exists(path("missing"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(kitzoo::os::is_regular(path("missing"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(kitzoo::os::is_dir(path("missing"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(kitzoo::os::is_link(path("missing"), ec));
  EXPECT_FALSE(ec);
}

TEST_F(FsTest, RemovePathDefaultsToNonRecursiveRemoval) {
  kitzoo::os::make_directory(path("tree/sub"), true);
  kitzoo::os::write_file(path("tree/sub/data"), "kept");
  std::error_code ec;
  EXPECT_EQ(kitzoo::os::remove_path(path("tree"), ec), 0u);
  EXPECT_TRUE(ec);
  EXPECT_EQ(kitzoo::os::read_file(path("tree/sub/data")), "kept");
  EXPECT_THROW(kitzoo::os::remove_path(path("tree")), std::filesystem::filesystem_error);
  EXPECT_EQ(kitzoo::os::remove_path(path("tree"), true), 3u);
  EXPECT_FALSE(kitzoo::os::path_exists(path("tree")));
  EXPECT_EQ(kitzoo::os::remove_path(path("tree"), ec), 0u);
  EXPECT_FALSE(ec);
  EXPECT_EQ(kitzoo::os::remove_path(path("tree"), true), 0u);
}

TEST_F(FsTest, RemovePathDeletesFilesAndEmptyDirectories) {
  kitzoo::os::write_file(path("file"), "x");
  kitzoo::os::make_directory(path("empty"));
  EXPECT_EQ(kitzoo::os::remove_path(path("file")), 1u);
  EXPECT_EQ(kitzoo::os::remove_path(path("empty")), 1u);
}

TEST_F(FsTest, ListDirectoryReturnsSortedPathsWithOptionalRecursion) {
  kitzoo::os::make_directory(path("child"));
  kitzoo::os::write_file(path("z.txt"), "z");
  kitzoo::os::write_file(path("a.txt"), "a");
  kitzoo::os::write_file(path("child/nested.txt"), "nested");
  const auto flat = kitzoo::os::list_directory(root_);
  EXPECT_EQ(flat, (std::vector<std::filesystem::path>{path("a.txt"), path("child"), path("z.txt")}));
  auto recursive = kitzoo::os::list_directory(root_, true);
  ASSERT_EQ(recursive.size(), 4u);
  EXPECT_TRUE(std::is_sorted(recursive.begin(), recursive.end()));
  EXPECT_NE(std::find(recursive.begin(), recursive.end(), path("child/nested.txt")), recursive.end());
  EXPECT_EQ(kitzoo::os::list_directory(root_), flat);
  EXPECT_EQ(kitzoo::os::list_directory(root_), flat);
}

TEST_F(FsTest, ListingFailureReturnsEmptyAndErrorOrThrows) {
  std::error_code ec;
  EXPECT_TRUE(kitzoo::os::list_directory(path("missing"), ec).empty());
  EXPECT_TRUE(ec);
  EXPECT_THROW(static_cast<void>(kitzoo::os::list_directory(path("missing"))), std::filesystem::filesystem_error);
  kitzoo::os::write_file(path("file"), "x");
  EXPECT_TRUE(kitzoo::os::list_directory(path("file"), ec, true).empty());
  EXPECT_TRUE(ec);
  EXPECT_TRUE(kitzoo::os::list_directory(root_, ec).size() == 1u);
  EXPECT_FALSE(ec);
}

TEST_F(FsTest, AppendCreatesFileAndPreservesExistingContent) {
  kitzoo::os::append_file(path("file"), "first");
  kitzoo::os::append_file(path("file"), std::string_view("\0second", 7));
  kitzoo::os::append_file(path("file"), "");
  EXPECT_EQ(kitzoo::os::read_file(path("file")), std::string("first\0second", 12));
  std::error_code ec;
  kitzoo::os::append_file(path("missing/child"), "x", ec);
  EXPECT_TRUE(ec);
  EXPECT_THROW(kitzoo::os::append_file(path("missing/child"), "x"), std::filesystem::filesystem_error);
}

TEST_F(FsTest, CopyRequiresExplicitOverwrite) {
  kitzoo::os::write_file(path("source"), "original");
  EXPECT_TRUE(kitzoo::os::copy_file_to(path("source"), path("copy")));
  EXPECT_EQ(kitzoo::os::read_file(path("copy")), "original");
  kitzoo::os::write_file(path("source"), "updated");
  std::error_code ec;
  EXPECT_FALSE(kitzoo::os::copy_file_to(path("source"), path("copy"), ec));
  EXPECT_TRUE(ec);
  EXPECT_EQ(kitzoo::os::read_file(path("copy")), "original");
  EXPECT_TRUE(kitzoo::os::copy_file_to(path("source"), path("copy"), ec, true));
  EXPECT_FALSE(ec);
  EXPECT_EQ(kitzoo::os::read_file(path("copy")), "updated");
  EXPECT_THROW(kitzoo::os::copy_file_to(path("missing"), path("copy")), std::filesystem::filesystem_error);
}

TEST_F(FsTest, RecursiveListingAndRemovalDoNotFollowDirectorySymlinks) {
  kitzoo::os::make_directory(path("outside"));
  kitzoo::os::write_file(path("outside/kept"), "data");
  kitzoo::os::make_directory(path("tree"));
  std::error_code ec;
  std::filesystem::create_directory_symlink(path("outside"), path("tree/link"), ec);
  if (ec)
    GTEST_SKIP() << "directory symlinks unavailable: " << ec.message();
  EXPECT_TRUE(kitzoo::os::is_link(path("tree/link")));
  EXPECT_TRUE(kitzoo::os::is_dir(path("tree/link")));
  EXPECT_EQ(kitzoo::os::list_directory(path("tree"), true).size(), 1u);
  EXPECT_EQ(kitzoo::os::remove_path(path("tree"), true), 2u);
  EXPECT_EQ(kitzoo::os::read_file(path("outside/kept")), "data");
}

TEST_F(FsTest, DanglingSymlinkExistsAsLinkButNotAsTarget) {
  std::error_code ec;
  std::filesystem::create_symlink(path("missing"), path("link"), ec);
  if (ec)
    GTEST_SKIP() << "file symlinks unavailable: " << ec.message();
  EXPECT_TRUE(kitzoo::os::is_link(path("link")));
  EXPECT_FALSE(kitzoo::os::path_exists(path("link")));
  EXPECT_FALSE(kitzoo::os::is_regular(path("link")));
  EXPECT_EQ(kitzoo::os::remove_path(path("link")), 1u);
}
} // namespace
