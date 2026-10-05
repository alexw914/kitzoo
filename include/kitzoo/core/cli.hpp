// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/cli.hpp
// Description: Re-exports cxxopts types and provides macros for option definitions and parsing.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_CLI_HPP
#define KITZOO_CORE_CLI_HPP

#include <cxxopts.hpp>

namespace kitzoo::core {

using Options = cxxopts::Options;
using ParseResult = cxxopts::ParseResult;
using cxxopts::value;

} // namespace kitzoo::core

// Expression macros preserve chaining and evaluate each argument once.
#define KZ_CLI_ADD_OPTIONS(options) ((options).add_options())

#define KZ_CLI_PARSE_OPTIONS(options, argc, argv) ((options).parse((argc), (argv)))

// Value expressions fit directly into Options::add_options() calls.
// Types are variadic to support template arguments containing commas.
#define KZ_CLI_VALUE(...) (::kitzoo::core::value<__VA_ARGS__>())

// The default uses cxxopts string syntax, followed by the option type.
#define KZ_CLI_DEFAULT(default_text, ...) (::kitzoo::core::value<__VA_ARGS__>()->default_value((default_text)))

#endif // KITZOO_CORE_CLI_HPP
