// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/os/fsadaptor_test.cpp
// Description: Verifies filesystem adaptor operations and error handling.
// -----------------------------------------------------------------------------

#include <kitzoo/os.hpp>

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
using kitzoo::os::FsAdaptor;

class FsAdaptorTest : public ::testing::Test {
protected:
  auto SetUp() -> void override { root_ = fs_.temp_directory(); }

  auto TearDown() -> void override {
    std::error_code ec;
    fs_.rm(root_, ec, true);
  }

  auto path(std::string_view name) const -> std::filesystem::path { return root_ / name; }

  FsAdaptor& fs_ = FsAdaptor::instance();
  std::filesystem::path root_;
};

TEST_F(FsAdaptorTest, PreservesBinaryAndText) {
  const std::string bytes("a\0b", 3);
  fs_.write_file(path("binary"), std::span(bytes.data(), bytes.size()));
  EXPECT_EQ(fs_.read_file(path("binary")), bytes);
  fs_.write_text(path("text"), "old");
  EXPECT_EQ(fs_.read_text(path("text")), "old");
  const std::string updated = "new";
  fs_.atomic_write(path("text"), std::span(updated.data(), updated.size()));
  EXPECT_EQ(fs_.read_text(path("text")), updated);
  EXPECT_EQ(fs_.file_size(path("text")), 3u);
}

TEST_F(FsAdaptorTest, ReadsEmptyAndLargeFilesAndReportsMissingFiles) {
  fs_.write_file(path("empty"), std::span<char const>{});
  EXPECT_TRUE(fs_.read_file(path("empty")).empty());
  const std::string data(10'000, '\xAB');
  fs_.write_file(path("large"), std::span(data.data(), data.size()));
  EXPECT_EQ(fs_.read_file(path("large")), data);

  std::error_code ec;
  EXPECT_TRUE(fs_.read_file(path("missing"), ec).empty());
  EXPECT_TRUE(ec);
  EXPECT_THROW(static_cast<void>(fs_.read_file(path("missing"))), std::filesystem::filesystem_error);
  EXPECT_THROW(static_cast<void>(fs_.file_size(path("missing"))), std::filesystem::filesystem_error);
}

TEST_F(FsAdaptorTest, AtomicWriteCreatesParentsAndPreservesFileOnFailure) {
  const std::string data = "content";
  fs_.atomic_write(path("nested/deep/file"), std::span(data.data(), data.size()));
  EXPECT_EQ(fs_.read_file(path("nested/deep/file")), data);
  fs_.write_text(path("parent"), "existing file");

  std::error_code ec;
  fs_.atomic_write(path("parent/file"), std::span<char const>{}, ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(fs_.read_text(path("parent")), "existing file");
  EXPECT_EQ(fs_.listdir(root_).size(), 2u);
}

TEST_F(FsAdaptorTest, TempDirectoriesAreUnique) {
  auto const directory = fs_.temp_directory();
  EXPECT_NE(directory, root_);
  EXPECT_TRUE(fs_.is_folder(directory));
  fs_.rm(directory);
}

TEST_F(FsAdaptorTest, UnicodePathsSupportAtomicReplacement) {
  const auto filename = root_ / std::filesystem::path(u8"\u6d4b\u8bd5/\u6570\u636e.txt");
  const std::string initial = "initial";
  fs_.atomic_write(filename, std::span(initial.data(), initial.size()));
  const std::string updated = "updated";
  fs_.atomic_write(filename, std::span(updated.data(), updated.size()));
  fs_.append_text(filename, "!");
  EXPECT_EQ(fs_.read_text(filename), "updated!");
  EXPECT_EQ(fs_.listdir(filename.parent_path()).size(), 1u);
}

TEST_F(FsAdaptorTest, MkdirCreatesOnlyRequestedLevelUnlessParentsEnabled) {
  std::error_code ec = std::make_error_code(std::errc::io_error);
  EXPECT_TRUE(fs_.mkdir(path("one"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(fs_.mkdir(path("one")));
  EXPECT_FALSE(fs_.mkdir(path("missing/deep"), ec));
  EXPECT_TRUE(ec);
  EXPECT_TRUE(fs_.mkdir(path("missing/deep"), ec, true));
  EXPECT_FALSE(ec);
  EXPECT_TRUE(fs_.is_folder(path("missing/deep")));
  EXPECT_FALSE(fs_.create_directories(path("missing/deep")));
}

TEST_F(FsAdaptorTest, MkdirRejectsExistingFile) {
  fs_.write_text(path("file"), "content");
  std::error_code ec;
  EXPECT_FALSE(fs_.mkdir(path("file"), ec));
  EXPECT_TRUE(ec);
  EXPECT_THROW(fs_.mkdir(path("file")), std::filesystem::filesystem_error);
}

TEST_F(FsAdaptorTest, PredicatesDistinguishFilesFoldersAndMissingPaths) {
  fs_.write_text(path("empty"), "");
  fs_.mkdir(path("folder"));
  EXPECT_TRUE(fs_.exists(path("empty")));
  EXPECT_TRUE(fs_.is_file(path("empty")));
  EXPECT_FALSE(fs_.is_folder(path("empty")));
  EXPECT_TRUE(fs_.is_empty(path("empty")));
  EXPECT_TRUE(fs_.is_folder(path("folder")));
  EXPECT_FALSE(fs_.is_file(path("folder")));
  EXPECT_TRUE(fs_.is_empty(path("folder")));
  fs_.write_text(path("folder/data"), "x");
  EXPECT_FALSE(fs_.is_empty(path("folder")));
  std::error_code ec = std::make_error_code(std::errc::io_error);
  EXPECT_FALSE(fs_.exists(path("missing"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(fs_.is_file(path("missing"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(fs_.is_folder(path("missing"), ec));
  EXPECT_FALSE(ec);
  EXPECT_FALSE(fs_.is_symlink(path("missing"), ec));
  EXPECT_FALSE(ec);
}

TEST_F(FsAdaptorTest, RmDefaultsToNonRecursiveRemoval) {
  fs_.mkdir(path("tree/sub"), true);
  fs_.write_text(path("tree/sub/data"), "kept");
  std::error_code ec;
  EXPECT_EQ(fs_.rm(path("tree"), ec), 0u);
  EXPECT_TRUE(ec);
  EXPECT_EQ(fs_.read_text(path("tree/sub/data")), "kept");
  EXPECT_THROW(fs_.rm(path("tree")), std::filesystem::filesystem_error);
  EXPECT_EQ(fs_.rm(path("tree"), true), 3u);
  EXPECT_FALSE(fs_.exists(path("tree")));
  EXPECT_EQ(fs_.rm(path("tree"), ec), 0u);
  EXPECT_FALSE(ec);
  EXPECT_EQ(fs_.rm(path("tree"), true), 0u);
}

TEST_F(FsAdaptorTest, RmDeletesFilesAndEmptyDirectories) {
  fs_.write_text(path("file"), "x");
  fs_.mkdir(path("empty"));
  EXPECT_EQ(fs_.rm(path("file")), 1u);
  EXPECT_EQ(fs_.rm(path("empty")), 1u);
}

TEST_F(FsAdaptorTest, ListdirReturnsSortedPathsWithOptionalRecursion) {
  fs_.mkdir(path("child"));
  fs_.write_text(path("z.txt"), "z");
  fs_.write_text(path("a.txt"), "a");
  fs_.write_text(path("child/nested.txt"), "nested");
  const auto flat = fs_.listdir(root_);
  EXPECT_EQ(flat, (std::vector<std::filesystem::path>{path("a.txt"), path("child"), path("z.txt")}));
  auto recursive = fs_.listdir(root_, true);
  ASSERT_EQ(recursive.size(), 4u);
  EXPECT_TRUE(std::is_sorted(recursive.begin(), recursive.end()));
  EXPECT_NE(std::find(recursive.begin(), recursive.end(), path("child/nested.txt")), recursive.end());
  EXPECT_EQ(fs_.list_directory(root_), flat);
  EXPECT_EQ(fs_.list_directory(root_), flat);
}

TEST_F(FsAdaptorTest, ListingFailureReturnsEmptyAndErrorOrThrows) {
  std::error_code ec;
  EXPECT_TRUE(fs_.listdir(path("missing"), ec).empty());
  EXPECT_TRUE(ec);
  EXPECT_THROW(fs_.listdir(path("missing")), std::filesystem::filesystem_error);
  fs_.write_text(path("file"), "x");
  EXPECT_TRUE(fs_.listdir(path("file"), ec, true).empty());
  EXPECT_TRUE(ec);
  EXPECT_TRUE(fs_.listdir(root_, ec).size() == 1u);
  EXPECT_FALSE(ec);
}

TEST_F(FsAdaptorTest, AppendCreatesFileAndPreservesExistingContent) {
  fs_.append_text(path("file"), "first");
  fs_.append_text(path("file"), std::string_view("\0second", 7));
  fs_.append_text(path("file"), "");
  EXPECT_EQ(fs_.read_text(path("file")), std::string("first\0second", 12));
  std::error_code ec;
  fs_.append_text(path("missing/child"), "x", ec);
  EXPECT_TRUE(ec);
  EXPECT_THROW(fs_.append_text(path("missing/child"), "x"), std::filesystem::filesystem_error);
}

TEST_F(FsAdaptorTest, CopyRequiresExplicitOverwrite) {
  fs_.write_text(path("source"), "original");
  EXPECT_TRUE(fs_.copy_file(path("source"), path("copy")));
  EXPECT_EQ(fs_.read_text(path("copy")), "original");
  fs_.write_text(path("source"), "updated");
  std::error_code ec;
  EXPECT_FALSE(fs_.copy_file(path("source"), path("copy"), ec));
  EXPECT_TRUE(ec);
  EXPECT_EQ(fs_.read_text(path("copy")), "original");
  EXPECT_TRUE(fs_.copy_file(path("source"), path("copy"), ec, true));
  EXPECT_FALSE(ec);
  EXPECT_EQ(fs_.read_text(path("copy")), "updated");
  EXPECT_THROW(fs_.copy_file(path("missing"), path("copy")), std::filesystem::filesystem_error);
}

TEST_F(FsAdaptorTest, RenamePreservesContentAndReportsMissingSource) {
  fs_.write_text(path("before"), "data");
  fs_.rename(path("before"), path("after"));
  EXPECT_FALSE(fs_.exists(path("before")));
  EXPECT_EQ(fs_.read_text(path("after")), "data");
  std::error_code ec;
  fs_.rename(path("missing"), path("unused"), ec);
  EXPECT_TRUE(ec);
  EXPECT_THROW(fs_.rename(path("missing"), path("unused")), std::filesystem::filesystem_error);
}

TEST_F(FsAdaptorTest, AbsoluteAndCanonicalResolveExistingPaths) {
  fs_.write_text(path("file"), "x");
  EXPECT_TRUE(fs_.absolute(".").is_absolute());
  EXPECT_EQ(fs_.canonical(root_ / "." / "file"), std::filesystem::canonical(path("file")));
  std::error_code ec;
  EXPECT_TRUE(fs_.canonical(path("missing"), ec).empty());
  EXPECT_TRUE(ec);
  EXPECT_THROW(fs_.canonical(path("missing")), std::filesystem::filesystem_error);
}

TEST_F(FsAdaptorTest, RecursiveListingAndRemovalDoNotFollowDirectorySymlinks) {
  fs_.mkdir(path("outside"));
  fs_.write_text(path("outside/kept"), "data");
  fs_.mkdir(path("tree"));
  std::error_code ec;
  std::filesystem::create_directory_symlink(path("outside"), path("tree/link"), ec);
  if (ec)
    GTEST_SKIP() << "directory symlinks unavailable: " << ec.message();
  EXPECT_TRUE(fs_.is_symlink(path("tree/link")));
  EXPECT_TRUE(fs_.is_folder(path("tree/link")));
  EXPECT_EQ(fs_.listdir(path("tree"), true).size(), 1u);
  EXPECT_EQ(fs_.rm(path("tree"), true), 2u);
  EXPECT_EQ(fs_.read_text(path("outside/kept")), "data");
}

TEST_F(FsAdaptorTest, DanglingSymlinkExistsAsLinkButNotAsTarget) {
  std::error_code ec;
  std::filesystem::create_symlink(path("missing"), path("link"), ec);
  if (ec)
    GTEST_SKIP() << "file symlinks unavailable: " << ec.message();
  EXPECT_TRUE(fs_.is_symlink(path("link")));
  EXPECT_FALSE(fs_.exists(path("link")));
  EXPECT_FALSE(fs_.is_file(path("link")));
  EXPECT_EQ(fs_.rm(path("link")), 1u);
}
} // namespace
