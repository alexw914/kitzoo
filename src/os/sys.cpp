// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/os/sys.cpp
// Description: Implements system queries, thread operations, and IPC path
//              allocation with Linux, macOS, and Windows backends.
// -----------------------------------------------------------------------------

#include <kitzoo/core/scopeguard.hpp>
#include <kitzoo/memory/memory.hpp>
#include <kitzoo/os/sys.hpp>

#include <cstdint>
#include <cstdlib>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <process.h>
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#endif

namespace kitzoo::os {

auto get_env(const std::string_view name) -> std::optional<std::string> {
  const memory::String key{name};
  if (const char* value = std::getenv(key.c_str())) {
    return std::string{value};
  }
  return std::nullopt;
}

auto hostname() -> std::string {
#if defined(_WIN32)
  char buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD size = sizeof(buf);

  if (GetComputerNameA(buf, &size) != 0)
    return std::string{buf, size};
  return {};
#else
  char buf[256] = {};
  if (::gethostname(buf, sizeof(buf) - 1) == 0)
    return std::string{buf};
  return {};
#endif
}

auto cpu_count() noexcept -> unsigned int {
  return std::thread::hardware_concurrency();
}

auto current_pid() noexcept -> long {
#if defined(_WIN32)
  return static_cast<long>(::_getpid());
#else
  return static_cast<long>(::getpid());
#endif
}

auto page_size() noexcept -> std::size_t {
#if defined(_WIN32)
  SYSTEM_INFO info;
  ::GetSystemInfo(&info);
  return static_cast<std::size_t>(info.dwPageSize);
#else
  const auto ps = ::sysconf(_SC_PAGESIZE);
  return ps > 0 ? static_cast<std::size_t>(ps) : 0;
#endif
}

auto total_memory() noexcept -> std::uint64_t {
#if defined(_WIN32)
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  if (::GlobalMemoryStatusEx(&status)) {
    return static_cast<std::uint64_t>(status.ullTotalPhys);
  }
  return 0;
#elif defined(__APPLE__)
  std::uint64_t memory = 0;
  std::size_t size = sizeof(memory);
  return ::sysctlbyname("hw.memsize", &memory, &size, nullptr, 0) == 0 ? memory : 0;
#else
  const auto pages = ::sysconf(_SC_PHYS_PAGES);
  const auto ps = ::sysconf(_SC_PAGESIZE);
  if (pages <= 0 || ps <= 0)
    return 0;
  return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(ps);
#endif
}

auto username() -> std::string {
  if (const auto env = get_env("USER"))
    return *env;
  if (const auto env = get_env("USERNAME"))
    return *env;
#if !defined(_WIN32)
  char buf[256] = {};
  if (::getlogin_r(buf, sizeof(buf) - 1) == 0)
    return std::string{buf};
#endif
  return {};
}

auto home_dir() -> std::string {
  if (const auto env = get_env("HOME"))
    return *env;
  if (const auto env = get_env("USERPROFILE"))
    return *env;
  return {};
}

auto stacktrace(const int max_frames) -> std::vector<std::string> {
  if (max_frames <= 0)
    return {};
#if defined(_WIN32)
  // The capture API returns a USHORT frame count. Windows provides addresses;
  // symbol resolution would additionally require DbgHelp and symbol files.
  const auto capacity = static_cast<DWORD>(max_frames > 65535 ? 65535 : max_frames);
  memory::Vector<void*> frames(capacity);
  const auto count = ::CaptureStackBackTrace(0, capacity, frames.data(), nullptr);
  std::vector<std::string> out;
  out.reserve(count);
  for (USHORT i = 0; i < count; ++i) {
    std::ostringstream address;
    address << frames[i];
    out.push_back(address.str());
  }
  return out;
#else
  memory::Vector<void*> frames(static_cast<std::size_t>(max_frames));
  const int n = ::backtrace(frames.data(), static_cast<int>(frames.size()));
  if (n <= 0)
    return {};

  char** symbols = ::backtrace_symbols(frames.data(), n);
  if (symbols == nullptr)
    return {};
  KZ_SCOPE_EXIT {
    std::free(symbols);
  };

  std::vector<std::string> out;
  out.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    Dl_info info{};
    if (::dladdr(frames[static_cast<std::size_t>(i)], &info) == 0 || info.dli_sname == nullptr) {
      out.emplace_back(symbols[i]);
      continue;
    }
    int status = 0;
    char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
    KZ_SCOPE_EXIT {
      std::free(demangled);
    };
    std::string entry = status == 0 ? demangled : info.dli_sname;
    const auto offset =
        static_cast<const char*>(frames[static_cast<std::size_t>(i)]) - static_cast<const char*>(info.dli_saddr);
    entry += " + " + std::to_string(offset);
    out.push_back(std::move(entry));
  }
  return out;
#endif
}

auto get_cpu_timestamp_ns() -> std::uint64_t {
#if defined(_WIN32)
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    return 0;
  auto ticks = [](const FILETIME& time) -> std::uint64_t {
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
  };
  return (ticks(kernel) + ticks(user)) * 100ULL;
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return 0;
  }
  auto seconds = static_cast<std::uint64_t>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec);
  auto micros = static_cast<std::uint64_t>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec);
  return seconds * 1000000000ULL + micros * 1000ULL;
#endif
}

auto set_thread_name(std::thread::native_handle_type id, std::string_view name, std::string_view requested_prefix)
    -> bool {
  if (name.empty())
    return false;
  std::string prefix{requested_prefix};
  if (prefix.empty())
    prefix = kThreadNamePrefix;
  else if (prefix.back() != '/')
    prefix += '/';
  if (prefix.size() > kMaxThreadNameLength)
    return false;
  auto trimmed = name;
  const auto budget = kMaxThreadNameLength - prefix.size();
  if (trimmed.size() > budget) {
    auto last = trimmed.rfind('/');
    trimmed = trimmed.substr(last == std::string_view::npos ? 0 : last + 1, budget);
  } else {
    auto first = trimmed.find_first_not_of('/');
    if (first == std::string_view::npos)
      return false;
    trimmed.remove_prefix(first);
  }
  prefix.append(trimmed);
  const auto& final_name = prefix;
#if defined(_WIN32)
  const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, final_name.c_str(), -1, nullptr, 0);
  if (size == 0)
    return false;
  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, final_name.c_str(), -1, wide.data(), size);
  using SetName = HRESULT(WINAPI*)(HANDLE, PCWSTR);
  auto function = reinterpret_cast<SetName>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"));
  return function && SUCCEEDED(function(id, wide.c_str()));
#elif defined(__APPLE__)
  return pthread_equal(id, pthread_self()) != 0 && pthread_setname_np(final_name.c_str()) == 0;
#else
  return pthread_setname_np(id, final_name.c_str()) == 0;
#endif
}

auto set_thread_name(std::thread& t, std::string_view n, std::string_view p) -> bool {
  return t.joinable() && set_thread_name(t.native_handle(), n, p);
}

auto set_current_thread_name(std::string_view n, std::string_view p) -> bool {
#if defined(_WIN32)
  return set_thread_name(GetCurrentThread(), n, p);
#else
  return set_thread_name(pthread_self(), n, p);
#endif
}

auto get_thread_name(std::thread::native_handle_type id) -> std::string {
#if defined(_WIN32)
  using GetName = HRESULT(WINAPI*)(HANDLE, PWSTR*);
  auto function = reinterpret_cast<GetName>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));
  PWSTR wide = nullptr;
  if (!function || FAILED(function(id, &wide)))
    return {};
  KZ_SCOPE_EXIT {
    LocalFree(wide);
  };
  const auto size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  std::string result;
  if (size > 0) {
    result.resize(static_cast<std::size_t>(size));
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
  }
  return result;
#else
  char name[64]{};
  int rc = pthread_getname_np(id, name, sizeof(name));
  if (rc) {
    return {};
  }
  return name;
#endif
}

auto get_thread_name(std::thread& t) -> std::string {
  return t.joinable() ? get_thread_name(t.native_handle()) : std::string{};
}

auto set_thread_priority(std::thread::native_handle_type id, std::int32_t priority, SchedPolicy requested) -> bool {
#if defined(_WIN32)
  if (requested != SchedPolicy::Other)
    return false;
  switch (priority) {
  case THREAD_PRIORITY_IDLE:
  case THREAD_PRIORITY_LOWEST:
  case THREAD_PRIORITY_BELOW_NORMAL:
  case THREAD_PRIORITY_NORMAL:
  case THREAD_PRIORITY_ABOVE_NORMAL:
  case THREAD_PRIORITY_HIGHEST:
  case THREAD_PRIORITY_TIME_CRITICAL:
    return SetThreadPriority(id, priority) != 0;
  default:
    return false;
  }
#else
  int policy = SCHED_OTHER;
  sched_param param{};
  if (pthread_getschedparam(id, &policy, &param) != 0)
    return false;
  if (requested == SchedPolicy::Rr)
    policy = SCHED_RR;
  else if (requested == SchedPolicy::Fifo)
    policy = SCHED_FIFO;
  else if (requested == SchedPolicy::Other)
    policy = SCHED_OTHER;
  else
    return false;
#if defined(__linux__)
  param.sched_priority = policy == SCHED_OTHER ? 0 : priority;
#else
  param.sched_priority = priority;
#endif
  if (param.sched_priority < sched_get_priority_min(policy) || param.sched_priority > sched_get_priority_max(policy))
    return false;
  return pthread_setschedparam(id, policy, &param) == 0;
#endif
}

auto set_thread_priority(std::thread& t, std::int32_t p, SchedPolicy s) -> bool {
  return t.joinable() && set_thread_priority(t.native_handle(), p, s);
}

auto set_current_thread_priority(std::int32_t p, SchedPolicy s) -> bool {
#if defined(_WIN32)
  return set_thread_priority(GetCurrentThread(), p, s);
#else
  return set_thread_priority(pthread_self(), p, s);
#endif
}

auto set_thread_nice(std::thread::native_handle_type id, std::uint32_t tid, std::int32_t nice) -> bool {
#if defined(__linux__)
  int policy = SCHED_OTHER;
  sched_param param{};
  if (nice < -20 || nice > 19 || tid == 0)
    return false;
  if (pthread_getschedparam(id, &policy, &param) != 0)
    return false;
  if (policy != SCHED_OTHER)
    return false;
  if (setpriority(PRIO_PROCESS, tid, nice) != 0)
    return false;
  errno = 0;
  int actual = getpriority(PRIO_PROCESS, tid);
  return !(actual == -1 && errno != 0) && actual == nice;
#else
  (void)id;
  (void)tid;
  (void)nice;
  return false;
#endif
}

auto set_current_thread_nice(std::int32_t n) -> bool {
#if defined(__linux__)
  return set_thread_nice(pthread_self(), static_cast<std::uint32_t>(syscall(SYS_gettid)), n);
#else
  (void)n;
  return false;
#endif
}

auto bind_cpus(std::thread::native_handle_type id, const std::vector<std::uint32_t>& cores) -> bool {
#if defined(_WIN32)
  DWORD_PTR mask = 0;
  for (auto core : cores) {
    if (core < sizeof(mask) * 8)
      mask |= DWORD_PTR{1} << core;
  }
  return mask != 0 && SetThreadAffinityMask(id, mask) != 0;
#elif defined(__APPLE__)
  // Darwin affinity tags are cache-sharing hints, not binding to CPU indices.
  (void)id;
  (void)cores;
  return false;
#else
  if (cores.empty())
    return false;
  long count = sysconf(_SC_NPROCESSORS_CONF);
  cpu_set_t mask;
  CPU_ZERO(&mask);
  bool accepted = false;
  for (auto core : cores) {
    if (static_cast<long>(core) >= count)
      continue;
    if (core >= CPU_SETSIZE)
      continue;
    CPU_SET(core, &mask);
    accepted = true;
  }
  if (!accepted)
    return false;
  return pthread_setaffinity_np(id, sizeof(mask), &mask) == 0;
#endif
}

auto bind_cpus(std::thread& t, const std::vector<std::uint32_t>& cores) -> bool {
  return t.joinable() && bind_cpus(t.native_handle(), cores);
}

auto bind_current_cpus(const std::vector<std::uint32_t>& cores) -> bool {
#if defined(_WIN32)
  return bind_cpus(GetCurrentThread(), cores);
#else
  return bind_cpus(pthread_self(), cores);
#endif
}

auto generate_unique_domain_socket_address() -> std::string {
#if defined(_WIN32)
  char directory[MAX_PATH + 1]{};
  const auto length = GetTempPathA(MAX_PATH + 1, directory);
  if (length == 0 || length > MAX_PATH)
    return {};
  char path[MAX_PATH + 1]{};
  if (GetTempFileNameA(directory, "kzo", 0, path) == 0)
    return {};
  return path;
#else
  char path[256]{};
#if defined(__APPLE__)
  std::snprintf(path, sizeof(path), "/tmp/kitzoo_ipc_XXXXXX");
#else
  std::snprintf(path, sizeof(path), "/dev/shm/kitzoo_ipc_XXXXXX");
#endif
  int fd = mkstemp(path);
  if (fd == -1)
    return {};
  close(fd);
  return path;
#endif
}

auto remove_unique_domain_socket_address(const std::string& p) -> bool {
#if defined(_WIN32)
  return DeleteFileA(p.c_str()) != 0;
#else
  return unlink(p.c_str()) == 0;
#endif
}

} // namespace kitzoo::os
