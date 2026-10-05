// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/sys.hpp
// Description: Declares system queries, thread operations, process CPU time,
//              and IPC path allocation.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_SYS_HPP
#define KITZOO_OS_SYS_HPP

#include <kitzoo/core/macro.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace kitzoo::os {

// POSIX policies; Windows supports Other with native Windows priority values.
enum class SchedPolicy : std::uint8_t {
  Other = 0,
  Rr,
  Fifo,
};

inline constexpr std::string_view kThreadNamePrefix{"kz/"};

// Longest thread name, prefix included; Linux limits names to 15 bytes.
#if defined(__linux__)
inline constexpr std::size_t kMaxThreadNameLength = 15;
#else
inline constexpr std::size_t kMaxThreadNameLength = 63;
#endif

KZ_NODISCARD auto get_env(std::string_view name) -> std::optional<std::string>;

KZ_NODISCARD auto hostname() -> std::string;

KZ_NODISCARD auto cpu_count() noexcept -> unsigned int;

KZ_NODISCARD auto current_pid() noexcept -> long;

KZ_NODISCARD auto page_size() noexcept -> std::size_t;

KZ_NODISCARD auto total_memory() noexcept -> std::uint64_t;

KZ_NODISCARD auto username() -> std::string;

KZ_NODISCARD auto home_dir() -> std::string;

// Captures up to max_frames entries; non-positive limits return an empty trace.
// POSIX entries show demangled symbols when available; Windows entries contain
// addresses, without symbol-name resolution.
KZ_NODISCARD auto stacktrace(int max_frames = 64) -> std::vector<std::string>;

// Process user + system CPU time, in nanoseconds; zero on failure.
KZ_NODISCARD auto get_cpu_timestamp_ns() -> std::uint64_t;

auto set_thread_name(std::thread& thread, std::string_view thread_name,
                     std::string_view name_prefix = kThreadNamePrefix) -> bool;

// Native handles must refer to live threads, not Linux kernel TIDs.
// macOS can set only the current thread's name; other threads return false.
auto set_thread_name(std::thread::native_handle_type thread_id, std::string_view thread_name,
                     std::string_view name_prefix = kThreadNamePrefix) -> bool;

auto set_current_thread_name(std::string_view thread_name, std::string_view name_prefix = kThreadNamePrefix) -> bool;

KZ_NODISCARD auto get_thread_name(std::thread& thread) -> std::string;

KZ_NODISCARD auto get_thread_name(std::thread::native_handle_type thread_id) -> std::string;

auto set_thread_priority(std::thread& thread, std::int32_t thread_priority,
                         SchedPolicy sched_policy = SchedPolicy::Other) -> bool;

auto set_thread_priority(std::thread::native_handle_type thread_id, std::int32_t thread_priority,
                         SchedPolicy sched_policy = SchedPolicy::Other) -> bool;

auto set_current_thread_priority(std::int32_t thread_priority, SchedPolicy sched_policy = SchedPolicy::Other) -> bool;

// Linux only; other platforms return false without changing process nice.
auto set_thread_nice(std::thread::native_handle_type thread_id, std::uint32_t tid, std::int32_t nice) -> bool;

auto set_current_thread_nice(std::int32_t nice) -> bool;

// Windows indices are relative to the thread's processor group.
// macOS does not support CPU index binding; returns false.
auto bind_cpus(std::thread& thread, const std::vector<std::uint32_t>& cpu_cores) -> bool;

auto bind_cpus(std::thread::native_handle_type thread_id, const std::vector<std::uint32_t>& cpu_cores) -> bool;

auto bind_current_cpus(const std::vector<std::uint32_t>& cpu_cores) -> bool;

// Creates a closed placeholder in /dev/shm (Linux), /tmp (macOS), or the
// Windows temporary directory; empty on failure.
// Unlink the placeholder before binding a socket; clean up the socket afterward.
KZ_NODISCARD auto generate_unique_domain_socket_address() -> std::string;

auto remove_unique_domain_socket_address(const std::string& address) -> bool;

} // namespace kitzoo::os

#endif // KITZOO_OS_SYS_HPP
