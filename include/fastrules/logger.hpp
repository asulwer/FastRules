#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <memory>

namespace fastrules {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

// Helper to compare log levels for filtering
[[nodiscard]] inline bool operator>=(LogLevel a, LogLevel b) {
    return static_cast<int>(a) >= static_cast<int>(b);
}

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string ruleId;
    std::chrono::steady_clock::time_point timestamp;
};

// Simple callback-based logger. Users provide a handler, we call it.
class Logger {
public:
    using Handler = std::function<void(const LogEntry&)>;

    Logger() = default;
    explicit Logger(Handler handler) : handler_(std::move(handler)) {}

    void setHandler(Handler handler) { handler_ = std::move(handler); }
    bool hasHandler() const { return handler_ != nullptr; }

    // Set minimum log level to filter out lower-priority messages
    void setMinLevel(LogLevel level) { minLevel_ = level; }
    [[nodiscard]] LogLevel getMinLevel() const { return minLevel_; }

    void log(LogLevel level, const std::string& message, const std::string& ruleId = "") {
        if (handler_ && level >= minLevel_) {
            handler_({level, message, ruleId, std::chrono::steady_clock::now()});
        }
    }

    void trace(const std::string& msg, const std::string& ruleId = "") { log(LogLevel::Trace, msg, ruleId); }
    void debug(const std::string& msg, const std::string& ruleId = "") { log(LogLevel::Debug, msg, ruleId); }
    void info(const std::string& msg, const std::string& ruleId = "") { log(LogLevel::Info, msg, ruleId); }
    void warning(const std::string& msg, const std::string& ruleId = "") { log(LogLevel::Warning, msg, ruleId); }
    void error(const std::string& msg, const std::string& ruleId = "") { log(LogLevel::Error, msg, ruleId); }
    void fatal(const std::string& msg, const std::string& ruleId = "") { log(LogLevel::Fatal, msg, ruleId); }

private:
    Handler handler_;
    LogLevel minLevel_ = LogLevel::Trace;  // Default: log everything
};

} // namespace fastrules
