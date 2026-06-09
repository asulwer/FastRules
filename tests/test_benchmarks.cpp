// test_benchmarks.cpp
// Performance benchmarks for FastRules core operations.
// These are micro-benchmarks using Catch2's benchmarking support.

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <fastrules.hpp>
#include <chrono>
#include <cmath>

using namespace fastrules;

// ============================================================================
// Rule compilation benchmarks
// ============================================================================

TEST_CASE("Benchmark rule compilation", "[benchmark][compilation]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "x > 0 and y < 100";
    rule.action = "result = x + y";

    BENCHMARK("compile simple rule") {
        rule.compile(engine);
    };
}

TEST_CASE("Benchmark workflow compilation", "[benchmark][compilation]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = "x > " + std::to_string(i);
        workflow.rules.push_back(rule);
    }

    BENCHMARK("compile 10-rule workflow") {
        workflow.compile(engine);
    };
}

// ============================================================================
// Rule execution benchmarks
// ============================================================================

TEST_CASE("Benchmark simple rule execution", "[benchmark][execution]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "x > 0";
    rule.action = "result = x * 2";
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;
    int x = 42;
    params.emplace_back("x", &x);

    BENCHMARK("execute simple rule") {
        return rule.execute(engine, ctx, params);
    };
}

TEST_CASE("Benchmark rule with type registration", "[benchmark][execution]") {
    LuaEngine engine;

    struct Point { double x = 0; double y = 0; };
    engine.registerType<Point>("Point", {
        {"x", offsetof(Point, x), "double"},
        {"y", offsetof(Point, y), "double"}
    });

    Rule rule;
    rule.id = 1;
    rule.expression = "math.sqrt(point.x * point.x + point.y * point.y) < 100";
    rule.action = "distance = math.sqrt(point.x * point.x + point.y * point.y)";
    rule.compile(engine);

    RuleContext ctx;
    Point point{3.0, 4.0};
    std::vector<RuleParameter> params;
    params.emplace_back("point", &point);

    BENCHMARK("execute rule with C++ type") {
        return rule.execute(engine, ctx, params);
    };
}

// ============================================================================
// Workflow execution benchmarks
// ============================================================================

TEST_CASE("Benchmark sequential workflow execution", "[benchmark][execution]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = "x > " + std::to_string(i);
        workflow.rules.push_back(rule);
    }

    workflow.compile(engine);

    int x = 100;
    std::vector<RuleParameter> params;
    params.emplace_back("x", &x);

    BENCHMARK("execute 5-rule sequential workflow") {
        return workflow.execute(engine, params);
    };
}

TEST_CASE("Benchmark parallel workflow execution", "[benchmark][execution]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = "x > " + std::to_string(i);
        workflow.rules.push_back(rule);
    }

    workflow.compile(engine);

    int x = 100;
    std::vector<RuleParameter> params;
    params.emplace_back("x", &x);

    BENCHMARK("execute 5-rule parallel workflow") {
        return workflow.executeParallel(engine, params);
    };
}

// ============================================================================
// Memory / allocation benchmarks
// ============================================================================
TEST_CASE("Benchmark rule memory footprint", "[benchmark][memory]") {
    LuaEngine engine;
    
    BENCHMARK_ADVANCED("rule memory: create & compile")(Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            Rule rule;
            rule.id = 1;
            rule.expression = "x > 0";
                    return sizeof(rule.id);
        });
    };
}

TEST_CASE("Benchmark workflow memory scaling", "[benchmark][memory]") {
    LuaEngine engine;
    
    // Pre-build workflow outside benchmark
    Workflow workflow;
    workflow.id = 1;
    
    for (int i = 0; i < 50; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = "x > " + std::to_string(i);
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    BENCHMARK("workflow memory: execute 50 rules") {
        int x = 100;
        std::vector<RuleParameter> params;
        params.emplace_back("x", &x);
        return workflow.execute(engine, params).size();
    };
}

// ============================================================================
// Cache benchmarks
// ============================================================================

TEST_CASE("Benchmark cached vs uncached execution", "[benchmark][cache]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "x > 0";
    rule.action = "result = x";
    rule.cacheDuration = std::chrono::milliseconds(1000);
    rule.compile(engine);

    RuleContext ctx;
    int x = 42;
    std::vector<RuleParameter> params;
    params.emplace_back("x", &x);

    // Warm up cache
    rule.execute(engine, ctx, params);

    BENCHMARK("execute cached rule (hit)") {
        return rule.execute(engine, ctx, params);
    };

    Rule uncachedRule;
    uncachedRule.id = 1;
    uncachedRule.expression = "x > 0";
    uncachedRule.action = "result = x";
    uncachedRule.compile(engine);

    BENCHMARK("execute uncached rule") {
        return uncachedRule.execute(engine, ctx, params);
    };
}

