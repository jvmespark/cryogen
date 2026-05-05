#include "cryogen/core/log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace cryogen::log {
namespace {

std::atomic<Severity> g_min{Severity::Info};
std::mutex            g_mu;

const char* severity_label(Severity s) {
    switch (s) {
        case Severity::Trace: return "TRACE";
        case Severity::Debug: return "DEBUG";
        case Severity::Info:  return "INFO ";
        case Severity::Warn:  return "WARN ";
        case Severity::Error: return "ERROR";
        case Severity::Fatal: return "FATAL";
    }
    return "?????";
}

} // namespace

void set_min_severity(Severity s) { g_min.store(s, std::memory_order_relaxed); }
Severity min_severity()           { return g_min.load(std::memory_order_relaxed); }

void emit(Severity s, std::string_view category, std::string&& msg) {
    using clk = std::chrono::system_clock;
    const auto now    = clk::now();
    const auto t      = clk::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()).count() % 1000;

    std::lock_guard<std::mutex> lk(g_mu);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ts[32];
    std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d.%03d",
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(millis));

    FILE* sink = (s >= Severity::Error) ? stderr : stdout;
    std::fprintf(sink, "[%s] [%s] [%.*s] %s\n",
                 ts, severity_label(s),
                 static_cast<int>(category.size()), category.data(),
                 msg.c_str());
    if (s == Severity::Fatal) std::fflush(sink);
}

} // namespace cryogen::log
