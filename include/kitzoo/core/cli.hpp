// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/cli.hpp
// Description: Re-exports cxxopts option types for command-line parsing.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_CLI_HPP
#define KITZOO_CORE_CLI_HPP

#include <cxxopts.hpp>

namespace kitzoo::core {

using Options = cxxopts::Options;
using ParseResult = cxxopts::ParseResult;
using cxxopts::value;

} // namespace kitzoo::core

#endif // KITZOO_CORE_CLI_HPP
