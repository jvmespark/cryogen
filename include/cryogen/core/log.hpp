#pragma once
//
// cryogen/core/log.hpp
// Lightweight logger. Thread-safe, severity-filtered, format-style.
//

#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cryogen::log {

enum class Severity { Trace = 0, Debug, Info, Warn, Error, Fatal };

void   set_min_severity(Severity s);
Severity min_severity();

void emit(Severity s, std::string_view category, std::string&& msg);

namespace detail {

inline void format_into(std::ostringstream&) {}

template <class T, class... R>
void format_into(std::ostringstream& os, T&& t, R&&... r) {
    os << std::forward<T>(t);
    format_into(os, std::forward<R>(r)...);
}

} // namespace detail

template <class... Args>
void log(Severity s, std::string_view category, Args&&... args) {
    if (s < min_severity()) return;
    std::ostringstream os;
    detail::format_into(os, std::forward<Args>(args)...);
    emit(s, category, os.str());
}

#define CRYOGEN_LOG(sev, cat, ...) ::cryogen::log::log(sev, cat, __VA_ARGS__)
#define CRYOGEN_TRACE(cat, ...) CRYOGEN_LOG(::cryogen::log::Severity::Trace, cat, __VA_ARGS__)
#define CRYOGEN_INFO(cat, ...)  CRYOGEN_LOG(::cryogen::log::Severity::Info,  cat, __VA_ARGS__)
#define CRYOGEN_WARN(cat, ...)  CRYOGEN_LOG(::cryogen::log::Severity::Warn,  cat, __VA_ARGS__)
#define CRYOGEN_ERROR(cat, ...) CRYOGEN_LOG(::cryogen::log::Severity::Error, cat, __VA_ARGS__)
#define CRYOGEN_FATAL(cat, ...) CRYOGEN_LOG(::cryogen::log::Severity::Fatal, cat, __VA_ARGS__)

} // namespace cryogen::log
