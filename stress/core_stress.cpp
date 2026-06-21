// core_stress.cpp
// Stress scenarios for the FastRules core engine.

#include "stress_runner.hpp"
#include "stress_helpers.hpp"
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

using namespace fastrules;
using namespace fastrules::stress;

namespace {

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

    StressConfig mixed = cfg;
    if (mixed.threads == 1) mixed.threads = std::thread::hardware_concurrency();
    if (mixed.threads == 0) mixed.threads = 4;

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.runConcurrent("compile + execute concurrent", mixed, [&](size_t worker, size_t iter) {
        auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
        wf.compile(*engine);
        if ((worker + iter) % 4 != 0) {
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

static StressResult largeWorkflowStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("large workflow", cfg, [&](size_t) {
        auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
}

static StressResult deepChildChainStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(1, 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("deep child-rule chain", cfg, [&](size_t) {
        auto wf = makeChainWorkflow(1, cfg.rules, cfg.parameters);
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
}

static StressResult actionThroughputStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);
    auto actionRef = engine->compileAction("return true");

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("action throughput", cfg, [&](size_t) {
        RuleContext ctx;
        engine->executeAction(*actionRef, params, ctx);
    });
}

static StressResult timeoutExecutorStorm(const StressConfig& cfg) {
    TimeoutExecutor executor(std::chrono::milliseconds(2));
    std::atomic<size_t> timeouts{0};

    StressRunner runner;
    return runner.run("timeout executor storm", cfg, [&](size_t) {
        try {
            executor.executeWithTimeout([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return 42;
            });
        } catch (const RuleTimeoutException&) {
            timeouts.fetch_add(1);
        }
    });
}

static StressResult executeAsyncBacklog(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner;
    return runner.run("executeAsync backlog", cfg, [&](size_t) {
        auto future = wf.executeAsync(*engine, params);
        (void)future.get();
    });
}

static StressResult coroutineChurn(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("coroutine churn", cfg, [&](size_t) {
        auto ref = engine->compileCoroutine("true");
        RuleContext ctx;
        engine->resumeCoroutine(*ref, params, ctx);
        engine->releaseRef(*ref);
    });
}

static StressResult typeRegistrationChurn(const StressConfig& cfg) {
    struct StressThing {
        int x = 0;
    };

    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("type registration churn", cfg, [&](size_t i) {
        engine->registerType<StressThing>(
            "StressThing_" + std::to_string(i),
            [](auto& reg) { reg.bind("x", &StressThing::x); });
    });
}

static StressResult parameterBloatStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    size_t paramCount = std::max(cfg.parameters, size_t(100));
    auto wf = makeBloatWorkflow(1, paramCount);
    auto params = makeParameters(static_cast<int>(paramCount), 0);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    auto result = runner.run("parameter bloat", cfg, [&](size_t) {
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
    result.note = "params=" + std::to_string(paramCount);
    return result;
}

static StressResult exceptionPathStress(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, 1, 1);
    wf.rules[0]->expression = "error('intentional failure')";

    StressRunner runner;
    return runner.run("exception path", cfg, [&](size_t) {
        try {
            wf.compile(*engine);
            auto params = makeParameters(1, 0);
            (void)wf.execute(*engine, params);
        } catch (const std::exception&) {
            // Expected; stress the exception propagation path.
        }
    });
}

static StressResult engineClonePressure(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
    wf.compile(*engine);

    StressRunner runner;
    return runner.run("engine clone pressure", cfg, [&](size_t) {
        auto clone = engine->clone();
        clone->setLogger(nullptr);
        (void)wf.execute(*clone, makeParameters(static_cast<int>(cfg.parameters), 0));
    });
}

static StressResult mixedWorkloadSoak(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 3);

    StressRunner runner([engine]() { return engine->getMemoryUsageKB(); });
    return runner.run("mixed workload soak", cfg, [&](size_t) {
        auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
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
}

int main(int argc, char* argv[]) {
    std::vector<std::function<StressResult(const StressConfig&)>> scenarios = {
        compileThroughput,
        executeThroughput,
        executeParallelStress,
        enginePoolExhaustion,
        concurrentCompileAndExecute,
        autoResetStress,
        largeWorkflowStress,
        deepChildChainStress,
        actionThroughputStress,
        timeoutExecutorStorm,
        executeAsyncBacklog,
        coroutineChurn,
        typeRegistrationChurn,
        parameterBloatStress,
        exceptionPathStress,
        engineClonePressure,
        mixedWorkloadSoak,
    };
    return runSuite("core", scenarios, argc, argv);
}
