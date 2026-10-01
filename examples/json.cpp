#include <kitzoo/json.hpp>

#include <cstdio>

int main() {
    auto parsed = kitzoo::json::parse(R"({"name":"kitzoo","workers":4})");
    auto const workers = kitzoo::json::get_at<int>(parsed, "workers");
    auto const mode = kitzoo::json::get_optional<std::string>(parsed, "mode").value_or("default");
    parsed["mode"] = mode;
    auto serialized = kitzoo::json::serialize(parsed);
    std::printf("workers=%d json=%s\n", workers, serialized.c_str());
}
