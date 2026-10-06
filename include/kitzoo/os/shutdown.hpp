// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/shutdown.hpp
// Description: Declares process-wide graceful shutdown requests triggered by
//              SIGINT/SIGTERM, console control events, or the program itself.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_SHUTDOWN_HPP
#define KITZOO_OS_SHUTDOWN_HPP

#include <kitzoo/core/macro.hpp>

#include <chrono>

namespace kitzoo::os {

// Installs handlers once: SIGINT/SIGTERM on POSIX, console control events on
// Windows. The first signal requests shutdown; a second one restores the default
// action so the process can still be terminated. Closing the Windows console
// window requests shutdown and holds the process open until main returns or
// Windows' close timeout (about 5 seconds) expires, so cleanup must be short.
auto install_shutdown_handler() -> void;

auto request_shutdown() noexcept -> void;

KZ_NODISCARD auto shutdown_requested() noexcept -> bool;

// Blocks until shutdown is requested; a zero timeout waits indefinitely.
// Returns whether shutdown was requested.
auto wait_for_shutdown(std::chrono::milliseconds timeout = {}) -> bool;

} // namespace kitzoo::os

#endif // KITZOO_OS_SHUTDOWN_HPP
