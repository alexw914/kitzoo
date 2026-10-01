// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory.hpp
// Description: Provides the public memory module entry point for optional
//              allocator integrations.
// -----------------------------------------------------------------------------

#pragma once

#if defined(KZ_WITH_MIMALLOC)
#include <kitzoo/memory/mimalloc_allocator.hpp>
#endif
