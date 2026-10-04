// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory.hpp
// Description: Provides the public memory module entry point for optional
//              allocator integrations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_HPP
#define KITZOO_MEMORY_HPP

#if defined(KZ_WITH_MIMALLOC)
#include <kitzoo/memory/mimalloc_allocator.hpp>
#endif

#endif  // KITZOO_MEMORY_HPP
