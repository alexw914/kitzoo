// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/cli/cli.hpp
// Description: Re-exports the commonly used cxxopts option, parse-result, and
//              value types in the kitzoo CLI namespace.
// -----------------------------------------------------------------------------

#pragma once

#include <cxxopts.hpp>

namespace kitzoo::cli {

using Options = cxxopts::Options;
using ParseResult = cxxopts::ParseResult;
using cxxopts::value;

}  // namespace kitzoo::cli
