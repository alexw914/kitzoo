// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/time/timestamp.cpp
// Description: Implements conversion of system-clock time points to formatted
//              timestamp strings.
// -----------------------------------------------------------------------------

#include <kitzoo/time/time.hpp>

#include <cstdio>
#include <ctime>

namespace kitzoo::time {

auto format_timestamp(std::chrono::system_clock::time_point const tp) -> std::string {
    auto const ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

    std::time_t const t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return std::string{buf};
}

}  // namespace kitzoo::time
