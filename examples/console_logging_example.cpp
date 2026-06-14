// console_logging_example.cpp
// Demonstrates how to configure spdlog for FastRules logging.

#include <fastrules.hpp>
#include <fastrules/logger.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <optional>

using namespace fastrules;

int main() {
    // 1. Configure spdlog at application startup
    // Pattern: [HH:MM:SS.msec] [level] message
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");

    // Set global log level (trace = everything, debug = no trace, etc.)
    spdlog::set_level(spdlog::level::debug);

    // Create a console sink with colors
    auto console = spdlog::stdout_color_mt("fastrules");
    spdlog::set_default_logger(console);

    // 2. Create engine and workflow
    LuaEngine engine;

    auto rule1 = Rule::create(1, "age >= 18")
        .withAction("eligible = true")
        .withPriority(1)
        .active(true)
        .build();

    Workflow workflow;
    workflow.id = 1;
    workflow.rules = {rule1};
    workflow.compile(engine);

    // 3. Execute -- all internal logging goes through spdlog
    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);

    auto results = workflow.execute(engine, params);

    // 4. Cleanup -- optional, spdlog handles its own shutdown
    spdlog::shutdown();

    return 0;
}
