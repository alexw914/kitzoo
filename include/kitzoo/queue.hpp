// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/queue.hpp
// Description: Provides the public queue module entry point for built-in queues
//              and their integration headers.
// -----------------------------------------------------------------------------

#ifndef KITZOO_QUEUE_HPP
#define KITZOO_QUEUE_HPP

// Choosing a queue:
// - BlockingQueue: general producer/consumer hand-off where consumers sleep
//   while empty, producers may need backpressure, or the stream must close.
// - SPSCQueue: a fixed pair of threads, such as a capture thread feeding a
//   processing thread; the fastest option, with polling instead of waiting.
// - MPMCQueue: many threads with a hard memory bound, no allocation after
//   construction, and strict FIFO; producers and consumers poll.
// - ConcurrentQueue/BlockingConcurrentQueue: high-throughput unbounded queues
//   where FIFO order across different producers is not required.

#include <kitzoo/queue/blocking_queue.hpp>
#include <kitzoo/queue/concurrent_queue.hpp>
#include <kitzoo/queue/mpmc_queue.hpp>
#include <kitzoo/queue/spsc_queue.hpp>

#endif // KITZOO_QUEUE_HPP
