// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: benchmarks/json_compare/compare.cpp
// Description: Compares reconstructed mjson and nlohmann JSON on identical inputs.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mjson/json.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

template <typename T>
auto consume(const T& value) -> void {
    asm volatile("" : : "g"(&value) : "memory");
}

template <typename Function>
auto measure(Function function, std::size_t iterations) -> double {
    const auto begin = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        function();
    }
    return std::chrono::duration<double, std::micro>(Clock::now() - begin).count() /
           static_cast<double>(iterations);
}

template <typename Left, typename Right>
auto compare(const std::string& fixture, const std::string& operation, std::size_t bytes, Left left,
             Right right) -> void {
    std::size_t iterations = 1;
    while (measure(left, iterations) * static_cast<double>(iterations) < 30000.0 &&
           iterations < 1048576) {
        iterations *= 2;
    }
    std::vector<double> left_samples, right_samples;
    for (int sample = 0; sample < 9; ++sample) {
        if (sample % 2 == 0) {
            left_samples.push_back(measure(left, iterations));
            right_samples.push_back(measure(right, iterations));
        } else {
            right_samples.push_back(measure(right, iterations));
            left_samples.push_back(measure(left, iterations));
        }
    }
    std::sort(left_samples.begin(), left_samples.end());
    std::sort(right_samples.begin(), right_samples.end());
    std::cout << fixture << ',' << operation << ',' << bytes << ',' << iterations << ','
              << left_samples[4] << ',' << right_samples[4] << ','
              << left_samples[4] / right_samples[4] << '\n';
}

auto run_fixture(const std::string& name, const Json& expected) -> void {
    const auto input = expected.dump();
    std::string error;
    const auto left_dom = mjson::Json::parse(input, error);
    const auto right_dom = Json::parse(input);
    if (!error.empty() || Json::parse(left_dom.dump()) != expected || right_dom != expected) {
        throw std::runtime_error("Fixture validation failed: " + name + ": " + error);
    }
    std::cerr << name << " input=" << input.size() << " mjson_dump=" << left_dom.dump().size()
              << " nlohmann_dump_ascii=" << right_dom.dump(-1, ' ', true).size() << '\n';
    compare(
        name, "parse_destroy", input.size(),
        [&]() -> void {
            std::string parse_error;
            auto value = mjson::Json::parse(input, parse_error);
            consume(value);
            consume(parse_error);
        },
        [&]() -> void {
            auto value = Json::parse(input);
            consume(value);
        });
    compare(
        name, "dump_ascii", input.size(),
        [&]() -> void {
            auto output = left_dom.dump();
            consume(output);
        },
        [&]() -> void {
            auto output = right_dom.dump(-1, ' ', true);
            consume(output);
        });
    if (name == "unicode_1000") {
        compare(
            name, "dump_default", input.size(),
            [&]() -> void {
                auto output = left_dom.dump();
                consume(output);
            },
            [&]() -> void {
                auto output = right_dom.dump();
                consume(output);
            });
    }
    if (expected.is_array() && !expected.empty() && expected[0].is_object()) {
        compare(
            name, "lookup_integer", input.size(),
            [&]() -> void {
                const auto value = left_dom[expected.size() / 2][std::string("id")].int_value();
                consume(value);
            },
            [&]() -> void {
                const auto value = right_dom[expected.size() / 2][std::string("id")].get<int>();
                consume(value);
            });
    }
}

auto main() -> int {
    setenv("MFR_DISABLE_LOG", "1", 1);
    std::cout << std::fixed << std::setprecision(6);
    std::cerr << "sizeof mjson=" << sizeof(mjson::Json) << " nlohmann=" << sizeof(Json) << '\n';
    std::cout
        << "fixture,operation,input_bytes,iterations,mjson_us,nlohmann_us,mjson_over_nlohmann\n";
    try {
        run_fixture("config", Json{{"name", "camera"},
                                   {"enabled", true},
                                   {"width", 1920},
                                   {"height", 1080},
                                   {"settings", {{"gain", 1.25}, {"port", 8080}}},
                                   {"tags", {"front", "capture"}},
                                   {"optional", nullptr}});
        for (const auto count : {100, 10000}) {
            auto records = Json::array();
            for (int i = 0; i < count; ++i) {
                records.push_back(Json{{"id", i},
                                       {"name", "record_" + std::to_string(i)},
                                       {"active", i % 2 == 0},
                                       {"score", 1.25},
                                       {"values", {i, i + 1, i + 2}}});
            }
            run_fixture("records_" + std::to_string(count), records);
        }
        auto integers = Json::array();
        auto floats = Json::array();
        auto unicode = Json::array();
        for (int i = 0; i < 100000; ++i) {
            integers.push_back(i);
        }
        for (int i = 0; i < 10000; ++i) {
            floats.push_back(static_cast<double>(i) + 0.125);
        }
        for (int i = 0; i < 1000; ++i) {
            unicode.push_back("相机配置：图像采集与处理 🌍 " + std::to_string(i));
        }
        run_fixture("integers_100000", integers);
        run_fixture("floats_10000", floats);
        run_fixture("unicode_1000", unicode);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
