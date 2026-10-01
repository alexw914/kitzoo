// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/core/version.cpp
// Description: Defines the build metadata values exposed by the generated core
//              version header.
// -----------------------------------------------------------------------------

#include <kitzoo/core/build_config.hpp>
#include <kitzoo/core/version.hpp>

namespace kitzoo {

build_info const& current_build_info() noexcept {
    static const build_info info{
        .lib_version = library_version,
        .version_str = version_string,
        .compiler = KITZOO_COMPILER_INFO,
#ifndef NDEBUG
        .is_debug = true,
#else
        .is_debug = false,
#endif
        .sanitizers = KITZOO_ENABLED_SANITIZERS,
    };
    return info;
}

}  // namespace kitzoo
