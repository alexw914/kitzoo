// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/json/reader.hpp
// Description: Loads JSON from text, files, or existing nlohmann/json values.
// -----------------------------------------------------------------------------

#ifndef KITZOO_JSON_READER_HPP
#define KITZOO_JSON_READER_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/os/fsadaptor.hpp>

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
  explicit Reader(std::string const& text) : Reader(std::string_view{text}) {}

  explicit Reader(char const* text) : Reader(std::string_view{text}) {}

  explicit Reader(std::filesystem::path const& path) { load_file(path); }

  // A failed load preserves the previous document and records the latest input error.
  auto parse(std::string_view text) -> bool {
    auto next = Json::parse(text.begin(), text.end(), nullptr, false);
    if (next.is_discarded()) {
      error_info_ = "Invalid JSON text";
      return false;
    }
    json_.swap(next);
    error_info_.clear();
    return true;
  }

  auto load_file(std::filesystem::path const& path) -> bool {
    std::error_code ec;
    auto text = os::FsAdaptor::instance().read_text(path, ec);
    if (ec) {
      error_info_ = ec.message();
      return false;
    }
    return parse(text);
  }

  KZ_NODISCARD auto raw() noexcept -> Json& { return json_; }

  KZ_NODISCARD auto raw() const noexcept -> Json const& { return json_; }

  KZ_NODISCARD auto is_parse_success() const noexcept -> bool { return error_info_.empty(); }

  KZ_NODISCARD auto error_info() const noexcept -> std::string const& { return error_info_; }

private:
  Json json_ = Json::object();
  std::string error_info_;
};

} // namespace kitzoo::json

#endif // KITZOO_JSON_READER_HPP
