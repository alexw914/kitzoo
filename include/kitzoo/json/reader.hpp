// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/json/reader.hpp
// Description: Loads JSON from text, files, or existing nlohmann/json values.
// -----------------------------------------------------------------------------

#ifndef KITZOO_JSON_READER_HPP
#define KITZOO_JSON_READER_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/os/filesys.hpp>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace kitzoo::json {

using Json = nlohmann::json;

// Intentionally header-only. JSON access and conversion use the native nlohmann API.
class Reader final {
public:
  Reader() = default;

  explicit Reader(Json value) : json_(std::move(value)) {
    if (json_.is_discarded())
      error_info_ = "Invalid JSON value";
  }

  explicit Reader(std::string_view text) { parse(text); }

  // Exact overloads distinguish JSON text from Json and filesystem::path conversions.
  explicit Reader(const std::string& text) : Reader(std::string_view{text}) {}

  explicit Reader(const char* text) : Reader(std::string_view{text}) {}

  explicit Reader(const std::filesystem::path& path) { load_file(path); }

  // A failed load preserves the previous document and records the latest input error.
  auto parse(std::string_view text) -> bool {
    auto next = Json::parse(text.begin(), text.end(), nullptr, false);
    if (next.is_discarded()) {
      // A second, failure-only pass recovers the positioned error message.
      ErrorCollector collector;
      Json::sax_parse(text.begin(), text.end(), &collector);
      error_info_ = collector.message.empty() ? "Invalid JSON text" : std::move(collector.message);
      return false;
    }
    json_.swap(next);
    error_info_.clear();
    return true;
  }

  auto load_file(const std::filesystem::path& path) -> bool {
    std::error_code ec;
    auto text = os::read_file(path, ec);
    if (ec) {
      error_info_.assign(ec.message());
      return false;
    }
    return parse(text);
  }

  KZ_NODISCARD auto raw() noexcept -> Json& { return json_; }

  KZ_NODISCARD auto raw() const noexcept -> const Json& { return json_; }

  KZ_NODISCARD auto is_parse_success() const noexcept -> bool { return error_info_.empty(); }

  KZ_NODISCARD auto error_info() const noexcept -> const std::string& { return error_info_; }

private:
  // Receives the error nlohmann would otherwise throw; parse events are ignored.
  struct ErrorCollector final : Json::json_sax_t {
    std::string message;

    auto null() -> bool override { return true; }

    auto boolean(bool) -> bool override { return true; }

    auto number_integer(Json::number_integer_t) -> bool override { return true; }

    auto number_unsigned(Json::number_unsigned_t) -> bool override { return true; }

    auto number_float(Json::number_float_t, const Json::string_t&) -> bool override { return true; }

    auto string(Json::string_t&) -> bool override { return true; }

    auto binary(Json::binary_t&) -> bool override { return true; }

    auto start_object(std::size_t) -> bool override { return true; }

    auto key(Json::string_t&) -> bool override { return true; }

    auto end_object() -> bool override { return true; }

    auto start_array(std::size_t) -> bool override { return true; }

    auto end_array() -> bool override { return true; }

    auto parse_error(std::size_t, const std::string&, const Json::exception& error) -> bool override {
      message = error.what();
      return false;
    }
  };

  Json json_ = Json::object();
  std::string error_info_;
};

} // namespace kitzoo::json

#endif // KITZOO_JSON_READER_HPP
