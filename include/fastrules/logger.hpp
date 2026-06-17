#pragma once

#include <spdlog/spdlog.h>
#include <string>

namespace fastrules {

// Convenience wrapper around spdlog for rule/workflow IDs.
// All formatting is done by spdlog -- no custom LogEntry or handler types.
//
// Usage:
//   auto log = fastrules::logger();
//   log->info("Workflow {} validated", workflow.id);
//   log->debug("Rule {} executed in {}ms", ruleId, elapsedMs);
//
// Configuration (at application startup):
//   spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
//   spdlog::set_level(spdlog::level::debug);
//   auto console = spdlog::stdout_color_mt("fastrules");
//   spdlog::set_default_logger(console);
//
// For file logging:
//   auto file_logger = spdlog::basic_logger_mt("fastrules_file", "logs/rules.log");
//   spdlog::set_default_logger(file_logger);
//
// For multiple sinks (console + file):
//   auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
//   auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/rules.log", true);
//   std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks = {console_sink, file_sink};
//   auto logger = std::make_shared<spdlog::logger>("fastrules", sinks.begin(), sinks.end());
//   spdlog::set_default_logger(logger);

inline std::shared_ptr<spdlog::logger> logger() {
    static auto log = spdlog::default_logger();
    return log;
}

} // namespace fastrules
