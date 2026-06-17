#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <format>

namespace fastrules {

// Stub logger for when spdlog is not available
// This is a minimal implementation that just outputs to std::cout/std::cerr

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

class Logger {
public:
    template<typename... Args>
    void trace(const std::string& fmt, Args&&... args) {
        log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args) {
        log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(const std::string& fmt, Args&&... args) {
        log(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args) {
        log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::string& fmt, Args&&... args) {
        log(LogLevel::Error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void critical(const std::string& fmt, Args&&... args) {
        log(LogLevel::Critical, fmt, std::forward<Args>(args)...);
    }

private:
    template<typename... Args>
    void log(LogLevel level, const std::string& fmt, Args&&... args) {
        // Simple stub - just print the format string
        // In a real implementation, we'd format the arguments
        (void)level;
        (void)fmt;
        ((void)args, ...);
    }
};

// Convenience function to get logger
inline std::shared_ptr<Logger> logger() {
    static auto log = std::make_shared<Logger>();
    return log;
}

} // namespace fastrules
