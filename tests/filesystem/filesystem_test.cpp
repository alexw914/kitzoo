// ---------------------------------------------------------------------------
// kitzoo/filesystem tests
// ---------------------------------------------------------------------------

#include <kitzoo/filesystem/filesystem.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace kitzoo::fs;

class FilesystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_ = temp_directory();
        ASSERT_FALSE(tmp_.empty());
    }

    void TearDown() override { std::filesystem::remove_all(tmp_); }

    auto make_path(std::string_view name) const -> std::filesystem::path { return tmp_ / name; }

    std::filesystem::path tmp_;
};

// -- read_file / write_file round-trip ----------------------------------------

TEST_F(FilesystemTest, WriteReadRoundTrip) {
    auto const path = make_path("test.bin");
    std::string const data = "hello world";
    write_file(path, std::span{data.data(), data.size()});

    auto const result = read_file(path);
    EXPECT_EQ(result, data);
}

TEST_F(FilesystemTest, ReadFileErrorCode) {
    auto const path = make_path("does_not_exist");
    std::error_code ec;
    auto const result = read_file(path, ec);
    EXPECT_TRUE(ec);
    EXPECT_TRUE(result.empty());
}

TEST_F(FilesystemTest, ReadFileThrows) {
    auto const path = make_path("does_not_exist");
    EXPECT_THROW({ static_cast<void>(read_file(path)); }, std::filesystem::filesystem_error);
}

TEST_F(FilesystemTest, WriteReadEmpty) {
    auto const path = make_path("empty.bin");
    write_file(path, std::span<char const>{});
    auto const result = read_file(path);
    EXPECT_TRUE(result.empty());
}

TEST_F(FilesystemTest, WriteReadLargeData) {
    auto const path = make_path("large.bin");
    std::string const data(10'000, '\xAB');
    write_file(path, std::span{data.data(), data.size()});
    auto const result = read_file(path);
    ASSERT_EQ(result.size(), data.size());
    EXPECT_EQ(result, data);
}

// -- write_text / read_text ---------------------------------------------------

TEST_F(FilesystemTest, WriteReadText) {
    auto const path = make_path("test.txt");
    write_text(path, "hello world\n");
    auto const result = read_text(path);
    EXPECT_EQ(result, "hello world\n");
}

// -- atomic_write -------------------------------------------------------------

TEST_F(FilesystemTest, AtomicWriteBasic) {
    auto const path = make_path("atomic.txt");
    std::string const data = "atomic content";
    atomic_write(path, std::span{data.data(), data.size()});
    auto const result = read_file(path);
    EXPECT_EQ(result, data);
}

TEST_F(FilesystemTest, AtomicWriteOverwrites) {
    auto const path = make_path("atomic_over.txt");
    write_file(path, std::span{std::string{"old"}.data(), 3u});
    std::string data = "new";
    atomic_write(path, std::span{data.data(), data.size()});
    auto const result = read_file(path);
    EXPECT_EQ(result, "new");
}

TEST_F(FilesystemTest, AtomicWriteCreatesParents) {
    auto const path = tmp_ / "sub" / "deep" / "atomic.txt";
    std::string const data = "data";
    atomic_write(path, std::span{data.data(), data.size()});
    EXPECT_TRUE(std::filesystem::exists(path));
    auto const result = read_file(path);
    EXPECT_EQ(result, "data");
}

TEST_F(FilesystemTest, AtomicWriteErrorCode) {
    std::error_code ec;
    atomic_write("/proc/$$$/impossible/path/file.txt", std::span<char const>{}, ec);
    EXPECT_TRUE(ec);
}

// -- temp_directory -----------------------------------------------------------

TEST_F(FilesystemTest, TempDirectoryExists) {
    auto dir = temp_directory();
    EXPECT_TRUE(std::filesystem::exists(dir));
    EXPECT_TRUE(std::filesystem::is_directory(dir));
    std::filesystem::remove_all(dir);
}

TEST_F(FilesystemTest, TempDirectoryUnique) {
    auto dir1 = temp_directory();
    auto dir2 = temp_directory();
    EXPECT_NE(dir1, dir2);
    std::filesystem::remove_all(dir1);
    std::filesystem::remove_all(dir2);
}

TEST_F(FilesystemTest, TempDirectoryNestedWrite) {
    auto dir = temp_directory();
    auto file_path = dir / "nested" / "file.txt";
    std::filesystem::create_directories(file_path.parent_path());
    write_text(file_path, "nested content");
    EXPECT_TRUE(std::filesystem::exists(file_path));
    std::filesystem::remove_all(dir);
}

// -- remove_all ---------------------------------------------------------------

TEST_F(FilesystemTest, RemoveAllRecursive) {
    auto dir = temp_directory();
    auto sub = dir / "a" / "b";
    std::filesystem::create_directories(sub);
    write_text(sub / "f.txt", "x");
    auto const removed = kitzoo::fs::remove_all(dir);
    EXPECT_GT(removed, 1u);
    EXPECT_FALSE(std::filesystem::exists(dir));
}

// -- list_directory / file_size -------------------------------------------------

TEST_F(FilesystemTest, ListDirectory) {
    write_text(make_path("a.txt"), "a");
    write_text(make_path("b.txt"), "b");
    std::filesystem::create_directory(tmp_ / "sub");

    auto const entries = list_directory(tmp_);
    EXPECT_EQ(entries.size(), 3u);
}

TEST_F(FilesystemTest, CurrentPathAndSetCurrentPath) {
    auto const original = current_path();
    set_current_path(tmp_);
    EXPECT_TRUE(std::filesystem::equivalent(current_path(), tmp_));
    set_current_path(original);
    EXPECT_TRUE(std::filesystem::equivalent(current_path(), original));
}

TEST_F(FilesystemTest, SetCurrentPathErrorCode) {
    std::error_code ec;
    set_current_path(make_path("missing"), ec);
    EXPECT_TRUE(ec);
}

TEST_F(FilesystemTest, CreateDirectories) {
    auto const path = make_path("nested/deep");
    EXPECT_TRUE(kitzoo::fs::create_directories(path));
    EXPECT_TRUE(std::filesystem::is_directory(path));
    EXPECT_FALSE(kitzoo::fs::create_directories(path));
}

TEST_F(FilesystemTest, ListDirectoryMissing) {
    std::error_code ec;
    auto const entries = list_directory(make_path("does_not_exist"), ec);
    EXPECT_TRUE(ec);
    EXPECT_TRUE(entries.empty());
}

TEST_F(FilesystemTest, FileSize) {
    write_text(make_path("sized.txt"), "12345");
    auto const size = kitzoo::fs::file_size(make_path("sized.txt"));
    EXPECT_EQ(size, 5u);
}

TEST_F(FilesystemTest, FileSizeMissing) {
    EXPECT_THROW(static_cast<void>(kitzoo::fs::file_size(make_path("nope"))),
                 std::filesystem::filesystem_error);
}

// -- write_file with vector data ----------------------------------------------

TEST_F(FilesystemTest, WriteReadFromVector) {
    auto const path = make_path("vec.bin");
    std::vector<char> const data{'a', 'b', 'c', 0, 'd'};
    write_file(path, std::span{data});
    auto const result = read_file(path);
    ASSERT_EQ(result.size(), data.size());
    EXPECT_EQ(result[0], 'a');
    EXPECT_EQ(result[3], '\0');
}
