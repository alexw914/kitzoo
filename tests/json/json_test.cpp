// ---------------------------------------------------------------------------
// kitzoo/json tests
// ---------------------------------------------------------------------------

#include <kitzoo/json/json.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

using namespace kitzoo;
using namespace kitzoo::json;

TEST(JsonTest, ParseObject) {
    auto value = parse(R"({"name":"kitzoo","version":1,"stable":false})");
    EXPECT_EQ(value["name"].get<std::string>(), "kitzoo");
    EXPECT_EQ(value["version"].get<int>(), 1);
    EXPECT_EQ(value["stable"].get<bool>(), false);
}

TEST(JsonTest, ParseInvalidJson) {
    EXPECT_THROW(static_cast<void>(parse("{not json")), nlohmann::json::parse_error);
}

TEST(JsonTest, ParseEmpty) {
    EXPECT_THROW(static_cast<void>(parse("")), nlohmann::json::parse_error);
}

TEST(JsonTest, ParseArray) {
    auto value = parse("[1,2,3]");
    EXPECT_EQ(value.size(), 3u);
}

TEST(JsonTest, GetOr) {
    auto j = parse(R"({"a": 1, "s": "hi"})");
    EXPECT_EQ(get_or<int>(j, "a", 0), 1);
    EXPECT_EQ(get_or<int>(j, "missing", 99), 99);
    EXPECT_EQ(get_or<std::string>(j, "s", ""), "hi");
    // wrong type -> fallback, no throw
    EXPECT_EQ(get_or<int>(j, "s", 7), 7);
}

TEST(JsonTest, GetAt) {
    auto j = parse(R"({"a": 42})");
    EXPECT_EQ(get_at<int>(j, "a"), 42);
    EXPECT_THROW(static_cast<void>(get_at<int>(j, "nope")), nlohmann::json::out_of_range);
    EXPECT_THROW(static_cast<void>(get_at<std::string>(j, "a")), nlohmann::json::type_error);
}

TEST(JsonTest, GetOptional) {
    auto j = parse(R"({"id":7,"name":"kitzoo"})");
    EXPECT_EQ(get_optional<int>(j, "id"), 7);
    EXPECT_FALSE(get_optional<int>(j, "missing"));
    EXPECT_FALSE(get_optional<int>(j, "name"));
}

TEST(JsonTest, RoundTrip) {
    Json j;
    j["numbers"] = {1, 2, 3};
    j["nested"]["key"] = "value";
    auto serialized = serialize(j);
    auto const& text = serialized;
    auto parsed = parse(text);
    EXPECT_EQ(parsed["nested"]["key"].get<std::string>(), "value");
}

TEST(JsonTest, SavesAndLoadsFile) {
    auto const dir = fs::temp_directory();
    auto const path = dir / "data.json";
    Json value{{"ready", true}, {"count", 3}};
    save_file(path, value);
    auto loaded = load_file(path);
    EXPECT_EQ(loaded, value);
    fs::remove_all(dir);
}
