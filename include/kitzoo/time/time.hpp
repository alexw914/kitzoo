// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/time.hpp
// Description: Declares Stopwatch, Deadline, and wall-clock timestamp helpers
//              for measuring and representing time.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

#include <chrono>
#include <string>

namespace kitzoo::time {

class Stopwatch {
public:
    Stopwatch() noexcept : start_{std::chrono::steady_clock::now()} {}

    auto reset() noexcept -> void { start_ = std::chrono::steady_clock::now(); }

    KZ_NODISCARD auto elapsed() const noexcept -> std::chrono::steady_clock::duration {
        return std::chrono::steady_clock::now() - start_;
    }

    template <typename Duration = std::chrono::milliseconds>
    KZ_NODISCARD auto elapsed_as() const noexcept -> Duration {
        return std::chrono::duration_cast<Duration>(elapsed());
    }

private:
    std::chrono::steady_clock::time_point start_;
};

class Deadline {
public:
    KZ_NODISCARD static auto after(std::chrono::nanoseconds d) noexcept -> Deadline {
        return Deadline{std::chrono::steady_clock::now() + d};
    }

    KZ_NODISCARD static auto at(std::chrono::steady_clock::time_point tp) noexcept -> Deadline {
        return Deadline{tp};
    }

    KZ_NODISCARD auto expired() const noexcept -> bool {
        return std::chrono::steady_clock::now() >= at_;
    }

    KZ_NODISCARD auto remaining() const noexcept -> std::chrono::steady_clock::duration {
        return at_ - std::chrono::steady_clock::now();
    }

    KZ_NODISCARD auto time_point() const noexcept -> std::chrono::steady_clock::time_point {
        return at_;
    }

private:
    explicit Deadline(std::chrono::steady_clock::time_point tp) noexcept : at_{tp} {}
    std::chrono::steady_clock::time_point at_;
};

KZ_NODISCARD auto format_timestamp(
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now()) -> std::string;

}  // namespace kitzoo::time
