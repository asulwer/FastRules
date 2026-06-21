// core_stress.cpp
// Stress scenarios for the FastRules core engine.

#include "stress_runner.hpp"
#include "stress_helpers.hpp"
#include <fastrules/lua_engine.hpp>
#include <fastrules/rule.hpp>
#include <fastrules/workflow.hpp>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

using namespace fastrules;
using namespace fastrules::stress;

static StressResult compileThroughput(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });

    return runner.run("compile throughput", cfg, [&](size_t) {
        auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
        wf.compile(*engine);
    });
}

static StressResult executeThroughput(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("execute throughput", cfg, [&](size_t) {
        (void)wf.execute(*engine, params);
    });
}

static StressResult executeParallelStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.runConcurrent("execute parallel", cfg, [&](size_t, size_t) {
        (void)wf.executeParallel(*engine, params);
    });
}

static StressResult enginePoolExhaustion(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressConfig overSubscribed = cfg;
    if (overSubscribed.threads == 1) overSubscribed.threads = std::thread::hardware_concurrency() * 4;
    if (overSubscribed.threads == 0) overSubscribed.threads = 8;

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.runConcurrent("engine pool exhaustion", overSubscribed, [&](size_t, size_t) {
        (void)wf.executeParallel(*engine, params);
    });
    result.note = "threads=" + std::to_string(overSubscribed.threads);
    return result;
}

static StressResult concurrentCompileAndExecute(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);
    std::atomic<size_t> compiledCount{0};

    StressConfig mixed = cfg;
    if (mixed.threads == 1) mixed.threads = std::thread::hardware_concurrency();
    if (mixed.threads == 0) mixed.threads = 4;

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.runConcurrent("compile + execute concurrent", mixed, [&](size_t worker, size_t iter) {
        if ((worker + iter) % 4 == 0) {
            auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
            wf.compile(*engine);
            compiledCount.fetch_add(1);
        } else {
            auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
            wf.compile(*engine);
            (void)wf.execute(*engine, params);
        }
    });
}

static StressResult autoResetStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    engine->setAutoResetThreshold(128); // very low threshold to force resets
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("auto-reset stress", cfg, [&](size_t) {
        auto wf = makeWorkflow(1, 5, cfg.parameters); // tiny workflow, repeated compile
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
}

int main(int argc, char* argv[]) {
    std::vector<std::function<StressResult(const StressConfig&)>> scenarios = {
        compileThroughput,
        executeThroughput,
        executeParallelStress,
        enginePoolExhaustion,
        concurrentCompileAndExecute,
        autoResetStress,
    };
    return runSuite("core", scenarios, argc, argv);
}
