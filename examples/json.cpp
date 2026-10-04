// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/json.cpp
// Description: Demonstrates JSON loading and native reads and writes of common value types.
// -----------------------------------------------------------------------------

#include <kitzoo/json.hpp>

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Optional arguments: input.json [output.json]. Input files use the schema below.
auto main(int argc, char** argv) -> int {
    kitzoo::json::Reader reader;
    auto const loaded = argc > 1 ? reader.load_file(argv[1]) : reader.parse(R"({
        "count": -3,
        "offset": -9007199254740991,
        "sequence": 18446744073709551615,
        "scale": 0.5,
        "threshold": 0.125,
        "enabled": true,
        "name": "kitzoo",
        "note": null,
        "ports": [80, 443],
        "metadata": {"region": "local"},
        "services": [{"workers": 4}]
    })");
    if (!loaded) {
        std::cerr << "JSON load failed: " << reader.error_info() << '\n';
        return 1;
    }

    try {
        auto& json = reader.raw();

        // Read scalar types with the native get<T>() API.
        auto const count = json.at("count").get<int>();
        auto const offset = json.at("offset").get<std::int64_t>();
        auto const sequence = json.at("sequence").get<std::uint64_t>();
        auto const scale = json.at("scale").get<float>();
        auto const threshold = json.at("threshold").get<double>();
        auto const enabled = json.at("enabled").get<bool>();
        auto const name = json.at("name").get<std::string>();
        auto const note_is_null = json.at("note").is_null();
        auto const mode = json.value("mode", std::string{"default"});

        // Read arrays, objects, and nested fields.
        auto const ports = json.at("ports").get<std::vector<int>>();
        auto const metadata = json.at("metadata").get<std::map<std::string, std::string>>();
        auto const pointer = kitzoo::json::Json::json_pointer("/services/0/workers");
        auto const workers = json.at(pointer).get<int>();

        std::cout << std::boolalpha << "int=" << count << " int64=" << offset
                  << " uint64=" << sequence << '\n'
                  << "float=" << scale << " double=" << threshold << " bool=" << enabled << '\n'
                  << "string=" << name << " null=" << note_is_null << " default=" << mode << '\n'
                  << "array_size=" << ports.size() << " object_region=" << metadata.at("region")
                  << " nested_workers=" << workers << '\n';

        // Write each value type directly, without Reader-specific conversion helpers.
        json["count"] = 10;
        json["offset"] = std::int64_t{-42};
        json["sequence"] = std::uint64_t{42};
        json["scale"] = 0.75F;
        json["threshold"] = 0.25;
        json["enabled"] = false;
        json["name"] = std::string{"updated"};
        json["note"] = nullptr;
        json["ports"] = std::vector<int>{8080, 8443};
        json["metadata"] = std::map<std::string, std::string>{{"region", "remote"}};
        json.at(pointer) = 8;
        json["mode"] = mode;

        auto const text = json.dump(2);
        std::cout << "Updated JSON:\n" << text << '\n';
        if (argc > 2) {
            std::error_code ec;
            kitzoo::os::write_text(argv[2], text, ec);
            if (ec) {
                std::cerr << "JSON save failed: " << ec.message() << '\n';
                return 1;
            }
        }
    } catch (kitzoo::json::Json::exception const& error) {
        // Schema/type errors are handled at the application boundary.
        std::cerr << "JSON access failed: " << error.what() << '\n';
        return 1;
    }
}
