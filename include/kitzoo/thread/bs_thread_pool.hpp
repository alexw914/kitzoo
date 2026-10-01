// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/bs_thread_pool.hpp
// Description: Re-exports the BS::thread_pool types used by the optional
//              thread-pool integration target.
// -----------------------------------------------------------------------------

#pragma once

#include <BS_thread_pool.hpp>
#include <cstddef>

namespace kitzoo::thread {

template <BS::tp Options = BS::tp::none>
using BSThreadPool = BS::thread_pool<Options>;

using BSLightThreadPool = BS::light_thread_pool;
using BSPriorityThreadPool = BS::priority_thread_pool;
using BSPauseThreadPool = BS::pause_thread_pool;
using BSWdcThreadPool = BS::wdc_thread_pool;

}  // namespace kitzoo::thread
