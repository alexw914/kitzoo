// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/memory/mimalloc_init.cpp
// Description: Keeps mimalloc usable until exit in executables that replace the
//              process allocator.
// -----------------------------------------------------------------------------

#include <mimalloc.h>

namespace {

// Shared-library destructors (OpenCV's, for one) still allocate after the
// executable's own destructors, where mimalloc would otherwise tear down.
// A process never needs that teardown.
[[gnu::constructor]] auto keep_mimalloc_until_exit() -> void {
  mi_option_set(mi_option_destroy_on_exit, 2);
}

} // namespace
