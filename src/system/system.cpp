// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/system/system.cpp
// Description: Implements operating-system and process information queries with
//              platform-specific backends.
// -----------------------------------------------------------------------------

#include <kitzoo/system/system.hpp>

#include <cstdlib>
#include <thread>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <execinfo.h>
#include <unistd.h>
#endif

namespace kitzoo::sys {

auto get_env(std::string_view const name) -> std::optional<std::string> {
    std::string const key{name};
    if (char const* value = std::getenv(key.c_str())) {
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
    auto const ps = ::sysconf(_SC_PAGESIZE);
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
#else
    auto const pages = ::sysconf(_SC_PHYS_PAGES);
    auto const ps = ::sysconf(_SC_PAGESIZE);
    if (pages <= 0 || ps <= 0)
        return 0;
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(ps);
#endif
}

auto username() -> std::string {
    if (auto const env = get_env("USER"))
        return *env;
    if (auto const env = get_env("USERNAME"))
        return *env;
#if !defined(_WIN32)
    char buf[256] = {};
    if (::getlogin_r(buf, sizeof(buf) - 1) == 0)
        return std::string{buf};
#endif
    return {};
}

auto home_dir() -> std::string {
    if (auto const env = get_env("HOME"))
        return *env;
    if (auto const env = get_env("USERPROFILE"))
        return *env;
    return {};
}

auto stacktrace(int const max_frames) -> std::vector<std::string> {
#if defined(_WIN32)
    (void)max_frames;
    return {};
#else
    std::vector<void*> frames(static_cast<std::size_t>(max_frames));
    int const n = ::backtrace(frames.data(), static_cast<int>(frames.size()));
    if (n <= 0)
        return {};

    char** symbols = ::backtrace_symbols(frames.data(), n);
    if (symbols == nullptr)
        return {};

    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.emplace_back(symbols[i]);
    std::free(symbols);
    return out;
#endif
}

}  // namespace kitzoo::sys
