// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/os/shutdown.cpp
// Description: Implements shutdown requests with an async-signal-safe self-pipe
//              on POSIX and a console control handler on Windows.
// -----------------------------------------------------------------------------

#include <kitzoo/os/shutdown.hpp>

#include <atomic>
#include <mutex>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <condition_variable>
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <system_error>
#include <unistd.h>
#endif

namespace kitzoo::os {

namespace {

std::atomic<bool> requested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

#if defined(_WIN32)

std::mutex wait_mutex;
std::condition_variable wake;

auto notify() noexcept -> void {
  {
    std::lock_guard lock{wait_mutex};
  }
  wake.notify_all();
}

auto WINAPI on_console_event(DWORD event) -> BOOL {
  if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT)
    return FALSE;
  // A repeated event falls through to the default handler, which terminates.
  if (requested.exchange(true))
    return FALSE;
  notify();
  return TRUE;
}

#else

// Never drained, so the read end stays readable and wakes every waiter.
int pipe_fds[2]{-1, -1};

auto wake_pipe() -> const int* {
  static std::once_flag once;
  std::call_once(once, [] {
    if (::pipe(pipe_fds) != 0)
      throw std::system_error{errno, std::generic_category(), "shutdown pipe"};
    for (const int fd : pipe_fds) {
      ::fcntl(fd, F_SETFD, FD_CLOEXEC);
      ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    }
  });
  return pipe_fds;
}

auto notify() noexcept -> void {
  const int saved = errno;
  const char byte = 1;
  [[maybe_unused]] const auto written = ::write(pipe_fds[1], &byte, 1);
  errno = saved;
}

extern "C" auto on_signal(int signal) -> void {
  if (requested.exchange(true)) {
    std::signal(signal, SIG_DFL);
    std::raise(signal);
    return;
  }
  notify();
}

#endif

} // namespace

auto install_shutdown_handler() -> void {
  static std::once_flag once;
  std::call_once(once, [] {
#if defined(_WIN32)
    if (!::SetConsoleCtrlHandler(on_console_event, TRUE))
      throw std::system_error{static_cast<int>(::GetLastError()), std::system_category(), "SetConsoleCtrlHandler"};
#else
    wake_pipe();
    struct sigaction action {};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    for (const int signal : {SIGINT, SIGTERM})
      if (::sigaction(signal, &action, nullptr) != 0)
        throw std::system_error{errno, std::generic_category(), "sigaction"};
#endif
  });
}

auto request_shutdown() noexcept -> void {
#if !defined(_WIN32)
  try {
    wake_pipe();
  } catch (...) {
    requested.store(true);
    return;
  }
#endif
  if (!requested.exchange(true))
    notify();
}

auto shutdown_requested() noexcept -> bool {
  return requested.load();
}

auto wait_for_shutdown(std::chrono::milliseconds timeout) -> bool {
  if (requested.load())
    return true;
#if defined(_WIN32)
  std::unique_lock lock{wait_mutex};
  auto ready = [] { return requested.load(); };
  if (timeout == std::chrono::milliseconds::zero()) {
    wake.wait(lock, ready);
    return true;
  }
  return wake.wait_for(lock, timeout, ready);
#else
  pollfd entry{wake_pipe()[0], POLLIN, 0};
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!requested.load()) {
    int wait_ms = -1;
    if (timeout != std::chrono::milliseconds::zero()) {
      const auto remaining =
          std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (remaining <= 0)
        return false;
      wait_ms = static_cast<int>(remaining);
    }
    if (::poll(&entry, 1, wait_ms) < 0 && errno != EINTR)
      throw std::system_error{errno, std::generic_category(), "poll"};
  }
  return true;
#endif
}

} // namespace kitzoo::os
