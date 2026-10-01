#include <kitzoo/string.hpp>

#include <cstdio>
#include <span>
#include <string>
#include <string_view>

int main() {
    using namespace kitzoo::str;
    std::string input = "  42,Hello  ";
    auto const separator = input.find(',');
    auto const number = to_number<int>(std::string_view{input}.substr(0, separator));
    auto const text = trim(std::string_view{input}.substr(separator + 1));
    auto const encoded = hex_encode(std::as_bytes(std::span{text.data(), text.size()}));
    auto const decoded = hex_decode(encoded);
    std::printf("number=%d text=%.*s upper=%s hex=%s decoded=%zu bytes\n", number.value(),
                static_cast<int>(text.size()), text.data(), to_upper(text).c_str(), encoded.c_str(),
                decoded.value().size());
}
