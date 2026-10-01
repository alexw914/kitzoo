// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/queue/concurrent_queue.hpp
// Description: Re-exports moodycamel ConcurrentQueue and
//              BlockingConcurrentQueue through kitzoo names.
// -----------------------------------------------------------------------------

#pragma once

#include <blockingconcurrentqueue.h>
#include <concurrentqueue.h>

namespace kitzoo::queue {

template <typename T, typename Traits = moodycamel::ConcurrentQueueDefaultTraits>
using ConcurrentQueue = moodycamel::ConcurrentQueue<T, Traits>;

template <typename T, typename Traits = moodycamel::ConcurrentQueueDefaultTraits>
using BlockingConcurrentQueue = moodycamel::BlockingConcurrentQueue<T, Traits>;

}  // namespace kitzoo::queue
