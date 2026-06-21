// core_stress.cpp
// Doctest-based stress scenarios for the FastRules core engine.

#include "stress_runner.hpp"
#include "stress_helpers.hpp"

#define DOCTEST_CONFIG_IMPLEMENT

#include <doctest/doctest.h>

#include <fastrules/lua_engine.hpp>
#include <fastrules/rule.hpp>
#include <fastrules/rule_context.hpp>
#include <fastrules/timeout_executor.hpp>
#include <fastrules/workflow.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

#include <spdlog/spdlog.h>

using namespace fastrules;
using namespace fastrules::stress;

namespace {

// Global workload configuration, populated by main() from command-line flags.
StressConfig g_config;

// Build a workflow whose Nth rule has all previous rules as children.
Workflow makeChainWorkflow(int id, size_t depth, size_t paramCount) {
    Workflow wf;
    wf.id = id;
    wf.name = "chain-workflow-" + std::to_string(id);
    std::shared_ptr<Rule> prev;
    for (size_t i = 0; i < depth; ++i) {
        auto rule = makeRule(static_cast<int>(id * 1000 + static_cast<int>(i) + 1),
                             static_cast<int>(paramCount));
        rule->expression = "p0 > " + std::to_string(i); // trivial expression
        if (prev) {
            rule->childRules.push_back(prev);
        }
        wf.rules.push_back(rule);
        prev = rule;
    }
    return wf;
}

// Build a workflow with a single rule that references many parameters.
Workflow makeBloatWorkflow(int id, size_t paramCount) {
    Workflow wf;
    wf.id = id;
    wf.name = "bloat-workflow-" + std::to_string(id);
    auto rule = makeRule(id * 1000, 1);
    rule->expression = "p0 > 0";
    wf.rules.push_back(rule);
    return wf;
}

} // namespace

TEST_CASE("compile throughput") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });

    auto result = runner.run("compile throughput", g_config, [&](size_t) {
        auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
        wf.compile(*engine);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("execute throughput") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("execute throughput", g_config, [&](size_t) {
        (void)wf.execute(*engine, params);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("execute parallel") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.runConcurrent("execute parallel", g_config, [&](size_t, size_t) {
        (void)wf.executeParallel(*engine, params);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("engine pool exhaustion") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressConfig overSubscribed = g_config;
    if (overSubscribed.threads == 1) overSubscribed.threads = std::thread::hardware_concurrency() * 4;
    if (overSubscribed.threads == 0) overSubscribed.threads = 8;

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.runConcurrent("engine pool exhaustion", overSubscribed, [&](size_t, size_t) {
        (void)wf.executeParallel(*engine, params);
    });
    result.note = "threads=" + std::to_string(overSubscribed.threads);
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("compile + execute concurrent") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressConfig mixed = g_config;
    if (mixed.threads == 1) mixed.threads = std::thread::hardware_concurrency();
    if (mixed.threads == 0) mixed.threads = 4;

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.runConcurrent("compile + execute concurrent", mixed, [&](size_t worker, size_t iter) {
        auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
        wf.compile(*engine);
        if ((worker + iter) % 4 != 0) {
            (void)wf.execute(*engine, params);
        }
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("auto-reset stress") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    engine->setAutoResetThreshold(128); // very low threshold to force resets
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("auto-reset stress", g_config, [&](size_t) {
        auto wf = makeWorkflow(1, 5, g_config.parameters); // tiny workflow, repeated compile
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("large workflow") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("large workflow", g_config, [&](size_t) {
        auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("deep child-rule chain") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(1, 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("deep child-rule chain", g_config, [&](size_t) {
        auto wf = makeChainWorkflow(1, g_config.rules, g_config.parameters);
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("action throughput") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);
    auto actionRef = engine->compileAction("return true");

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("action throughput", g_config, [&](size_t) {
        RuleContext ctx;
        engine->executeAction(*actionRef, params, ctx);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("timeout executor storm") {
    TimeoutExecutor executor(std::chrono::milliseconds(2));
    std::atomic<size_t> timeouts{0};

    StressRunner runner;
    auto result = runner.run("timeout executor storm", g_config, [&](size_t) {
        try {
            executor.executeWithTimeout([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return 42;
            });
        } catch (const RuleTimeoutException&) {
            timeouts.fetch_add(1);
        }
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("executeAsync backlog") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressRunner runner;
    auto result = runner.run("executeAsync backlog", g_config, [&](size_t) {
        auto future = wf.executeAsync(*engine, params);
        (void)future.get();
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("coroutine churn") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("coroutine churn", g_config, [&](size_t) {
        auto ref = engine->compileCoroutine("true");
        RuleContext ctx;
        engine->resumeCoroutine(*ref, params, ctx);
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("type registration churn") {
    struct StressThing {
        int x = 0;
    };

    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("type registration churn", g_config, [&](size_t i) {
        engine->registerType<StressThing>(
            "StressThing_" + std::to_string(i),
            [](auto& reg) { reg.bind("x", &StressThing::x); });
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("parameter bloat") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    size_t paramCount = std::max(g_config.parameters, size_t(100));
    auto wf = makeBloatWorkflow(1, paramCount);
    auto params = makeParameters(static_cast<int>(paramCount), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("parameter bloat", g_config, [&](size_t) {
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
    result.note = "params=" + std::to_string(paramCount);
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("exception path") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, 1, 1);
    wf.rules[0]->expression = "error('intentional failure')";

    StressRunner runner;
    auto result = runner.run("exception path", g_config, [&](size_t) {
        try {
            wf.compile(*engine);
            auto params = makeParameters(1, 0);
            (void)wf.execute(*engine, params);
        } catch (const std::exception&) {
            // Expected; stress the exception propagation path.
        }
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("engine clone pressure") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
    wf.compile(*engine);

    StressRunner runner;
    auto result = runner.run("engine clone pressure", g_config, [&](size_t) {
        auto clone = engine->clone();
        clone->setLogger(nullptr);
        (void)wf.execute(*clone, makeParameters(static_cast<int>(g_config.parameters), 0));
    });
    result.print();
    CHECK(result.errors == 0);
}

TEST_CASE("mixed workload soak") {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(g_config.parameters), 0);
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 3);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("mixed workload soak", g_config, [&](size_t) {
        auto wf = makeWorkflow(1, g_config.rules, g_config.parameters);
        switch (dist(rng)) {
            case 0:
                wf.compile(*engine);
                break;
            case 1:
                wf.compile(*engine);
                (void)wf.execute(*engine, params);
                break;
            case 2:
                (void)wf.executeParallel(*engine, params);
                break;
            case 3: {
                auto fut = wf.executeAsync(*engine, params);
                (void)fut.get();
                break;
            }
        }
    });
    result.print();
    CHECK(result.errors == 0);
}

int main(int argc, char* argv[]) {
    // Silence spdlog during stress runs; individual engine loggers are also
    // disabled per scenario, but Workflow validation/compilation use the global
    // logger.
    spdlog::set_level(spdlog::level::off);

    // Parse our workload flags and remove them so doctest doesn't complain.
    std::vector<std::string> remaining;
    remaining.emplace_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto consume = [&](size_t& out) {
            if (i + 1 < argc) {
                out = static_cast<size_t>(std::atoll(argv[++i]));
            }
        };
        auto consumeDouble = [&](double& out) {
            if (i + 1 < argc) {
                out = std::atof(argv[++i]);
            }
        };

        if (arg == "--duration")      consumeDouble(g_config.durationSeconds);
        else if (arg == "--iterations") consume(g_config.iterations);
        else if (arg == "--threads")    consume(g_config.threads);
        else if (arg == "--rules")      consume(g_config.rules);
        else if (arg == "--parameters") consume(g_config.parameters);
        else if (arg == "--auto-reset-kb") consume(g_config.autoResetThresholdKB);
        else remaining.push_back(arg);
    }

    std::vector<const char*> doctestArgv;
    doctestArgv.reserve(remaining.size());
    for (const auto& s : remaining) {
        doctestArgv.push_back(s.c_str());
    }

    doctest::Context context(static_cast<int>(doctestArgv.size()), doctestArgv.data());
    return context.run();
}
