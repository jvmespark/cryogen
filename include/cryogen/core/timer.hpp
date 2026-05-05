#pragma once
//
// cryogen/core/timer.hpp
// Monotonic timing primitives.
//

#include <chrono>

#include "cryogen/core/types.hpp"

namespace cryogen::core {

class Timer {
public:
    Timer();

    // Returns seconds elapsed since the previous call to tick(), and resets
    // the internal stamp. The first call returns 0.
    f64 tick();

    // Seconds since the timer was constructed.
    f64 elapsed() const;

    // Reset to the construction zero.
    void reset();

private:
    using clock = std::chrono::steady_clock;
    clock::time_point start_;
    clock::time_point last_;
};

class ScopedTimer {
public:
    explicit ScopedTimer(const char* label);
    ~ScopedTimer();

private:
    const char* label_;
    std::chrono::steady_clock::time_point begin_;
};

#if defined(CRYOGEN_PROFILE)
    #define CRYOGEN_PROFILE_SCOPE(name) ::cryogen::core::ScopedTimer _cgprof_##__LINE__(name)
#else
    #define CRYOGEN_PROFILE_SCOPE(name) (void)0
#endif

} // namespace cryogen::core
