// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/cli.cpp
// Description: Demonstrates CLI macros for option definitions, parsing, and help.
// -----------------------------------------------------------------------------

#include <kitzoo/core.hpp>

#include <cstdio>
#include <exception>
#include <string>

auto main(int argc, char const* const* argv) -> int {
  kitzoo::core::Options options{argv[0], "CLI module example"};
  options.set_tab_expansion(true);
  KZ_CLI_ADD_OPTIONS(options)("h,help", "Show help", KZ_CLI_VALUE(bool))(
      "n,name", "Name to greet", KZ_CLI_DEFAULT("world", std::string))("v,verbose", "Enable verbose output",
                                                                       KZ_CLI_VALUE(bool));

  try {
    auto const result = KZ_CLI_PARSE_OPTIONS(options, argc, argv);
    if (result["help"].as<bool>()) {
      std::puts(options.help().c_str());
      return 0;
    }
    std::printf("hello, %s%s\n", result["name"].as<std::string>().c_str(),
                result["verbose"].as<bool>() ? " (verbose)" : "");
  } catch (std::exception const& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 2;
  }
}
