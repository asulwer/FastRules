// console_logging_example.cpp
// Demonstrates how to wire a custom logger (console output) into FastRules.
// Any sink works: file, syslog, spdlog, etc. — just provide the callback.

#include <fastrules.hpp>
#include <fastrules/logger.hpp>
#include <iostream>

using namespace fastrules;

int main() {
    // 1. Set up global console logger — runs once at app startup
    setGlobalLogger([](const LogEntry& entry) {
        const char* levelStr = "?";
        switch (entry.level) {
            case LogLevel::Trace:   levelStr = "TRACE"; break;
            case LogLevel::Debug:   levelStr = "DEBUG"; break;
            case LogLevel::Info:    levelStr = "INFO";  break;
            case LogLevel::Warning: levelStr = "WARN";  break;
            case LogLevel::Error:   levelStr = "ERROR"; break;
            case LogLevel::Fatal:   levelStr = "FATAL"; break;
        }
        std::cout << "[" << levelStr << "] ";
        if (!entry.ruleId.empty()) std::cout << "[" << entry.ruleId << "] ";
        std::cout << entry.message << "\n";
    });

    // Optional: filter out Trace/Debug noise
    globalLogger().setMinLevel(LogLevel::Debug);

    // 2. Create engine and workflow
    LuaEngine engine;

    auto rule1 = Rule::Builder("age-check")
        .withExpression("age >= 18")
        .withAction("eligible = true")
        .withPriority(1)
        .active(true)
        .build();

    auto rule2 = Rule::Builder("name-check")
        .withExpression("string.len(name) > 0")
        .withPriority(2)
        .active(true)
        .build();

    Workflow workflow;
    workflow.id = "validation";
    workflow.description = "Customer validation";
    workflow.rules.push_back(rule1);
    workflow.rules.push_back(rule2);

    // 3. Execute — you'll see log output on console
    std::vector<RuleParameter> params;
    params.emplace_back("age", "int", std::any(25));
    params.emplace_back("name", "string", std::any(std::string("Alice")));

    auto results = workflow.execute(engine, params);

    std::cout << "\n=== Results ===\n";
    for (const auto& r : results) {
        std::cout << "Rule " << r.ruleId << ": " << (r.isSuccess() ? "PASS" : "FAIL") << "\n";
    }

    // 4. Clean up (optional — resets to silent)
    clearGlobalLogger();
    return 0;
}
