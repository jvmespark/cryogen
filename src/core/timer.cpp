#include "cryogen/core/timer.hpp"
#include "cryogen/core/log.hpp"

namespace cryogen::core {

Timer::Timer()
    : start_(clock::now()), last_(start_) {}

f64 Timer::tick() {
    const auto now  = clock::now();
    const f64  dt   = std::chrono::duration<f64>(now - last_).count();
    last_ = now;
    return dt;
}

f64 Timer::elapsed() const {
    return std::chrono::duration<f64>(clock::now() - start_).count();
}

void Timer::reset() {
    start_ = clock::now();
    last_  = start_;
}

ScopedTimer::ScopedTimer(const char* label)
    : label_(label), begin_(std::chrono::steady_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    const auto end = std::chrono::steady_clock::now();
    const auto us  = std::chrono::duration_cast<std::chrono::microseconds>(end - begin_).count();
    CRYOGEN_TRACE("profile", label_, " : ", us, " us");
}

} // namespace cryogen::core
