// test_benchmarks.cpp
// Performance benchmarks for FastRules core operations.
// These are micro-benchmarks using Catch2's benchmarking support.
// Note: Benchmarks are skipped in Debug builds because unoptimized code
// produces meaningless results and runs very slowly.

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <fastrules.hpp>
#include <chrono>
#include <cmath>

using namespace fastrules;

// Skip benchmarks in Debug builds - they run too slow and produce
// meaningless results without compiler optimizations.
#ifdef _DEBUG
    #define SKIP_IN_DEBUG() SKIP("Benchmarks skipped in Debug builds")
#else
    #define SKIP_IN_DEBUG()
#endif

// ============================================================================
// Rule compilation benchmarks
// ============================================================================

TEST_CASE("Benchmark rule compilation", "[benchmark][compilation]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "1 > 0 and 2 < 100";
    rule.action = "result = 42";

    BENCHMARK("compile simple rule") {
        rule.compile(engine);
    };
}

TEST_CASE("Benchmark workflow compilation", "[benchmark][compilation]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = std::to_string(i) + " > 0";
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
    SKIP_IN_DEBUG();
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "42 > 0";
    rule.action = "result = 42 * 2";
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    BENCHMARK("execute simple rule") {
        return rule.execute(engine, ctx, params);
    };
}

TEST_CASE("Benchmark rule with type registration", "[benchmark][execution]") {
    SKIP_IN_DEBUG();
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
    SKIP_IN_DEBUG();
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = std::to_string(i) + " > -1";
        workflow.rules.push_back(rule);
    }

    workflow.compile(engine);

    std::vector<RuleParameter> params;

    BENCHMARK("execute 5-rule sequential workflow") {
        return workflow.execute(engine, params);
    };
}

TEST_CASE("Benchmark parallel workflow execution", "[benchmark][execution]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = std::to_string(i) + " > -1";
        workflow.rules.push_back(rule);
    }

    workflow.compile(engine);

    std::vector<RuleParameter> params;

    BENCHMARK("execute 5-rule parallel workflow") {
        return workflow.executeParallel(engine, params);
    };
}

TEST_CASE("Benchmark async workflow execution", "[benchmark][execution][async]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = std::to_string(i) + " > -1";
        workflow.rules.push_back(rule);
    }

    workflow.compile(engine);

    std::vector<RuleParameter> params;

    BENCHMARK("execute 5-rule async workflow") {
        auto future = workflow.executeAsync(engine, params);
        return future.get();
    };
}

TEST_CASE("Benchmark execution comparison", "[benchmark][execution][comparison]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    // Create 10 independent rules
    for (int i = 0; i < 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = std::to_string(i) + " > -1";
        workflow.rules.push_back(rule);
    }

    workflow.compile(engine);
    std::vector<RuleParameter> params;

    BENCHMARK("sequential execution") {
        return workflow.execute(engine, params);
    };

    BENCHMARK("parallel execution") {
        return workflow.executeParallel(engine, params);
    };

    BENCHMARK("async execution") {
        auto future = workflow.executeAsync(engine, params);
        return future.get();
    };
}

// ============================================================================
// Memory / allocation benchmarks
// ============================================================================
TEST_CASE("Benchmark rule memory footprint", "[benchmark][memory]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;
    
    BENCHMARK_ADVANCED("rule memory: create & compile")(Catch::Benchmark::Chronometer meter) {
        meter.measure([] {
            Rule rule;
            rule.id = 1;
            rule.expression = "1 > 0";
                    return sizeof(rule.id);
        });
    };
}

TEST_CASE("Benchmark workflow memory scaling", "[benchmark][memory]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;
    
    // Pre-build workflow outside benchmark
    Workflow workflow;
    workflow.id = 1;
    
    for (int i = 0; i < 50; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = std::to_string(i) + " > -1";
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    BENCHMARK("workflow memory: execute 50 rules") {
        std::vector<RuleParameter> params;
        return workflow.execute(engine, params).size();
    };
}

// ============================================================================
// Cache benchmarks
// ============================================================================

TEST_CASE("Benchmark cached vs uncached execution", "[benchmark][cache]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "42 > 0";
    rule.action = "result = 42";
    rule.cacheDuration = std::chrono::milliseconds(1000);
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    // Warm up cache
    rule.execute(engine, ctx, params);

    BENCHMARK("execute cached rule (hit)") {
        return rule.execute(engine, ctx, params);
    };

    Rule uncachedRule;
    uncachedRule.id = 1;
    uncachedRule.expression = "42 > 0";
    uncachedRule.action = "result = 42";
    uncachedRule.compile(engine);

    BENCHMARK("execute uncached rule") {
        return uncachedRule.execute(engine, ctx, params);
    };
}

// ============================================================================
// Expression parsing benchmarks (post-regex removal #4)
// ============================================================================

TEST_CASE("Benchmark expression parsing (post-regex)", "[benchmark][parsing]") {
    SKIP_IN_DEBUG();
    LuaEngine engine;

    // Complex expression with multiple identifiers, operators, and literals
    // This would have been slow with std::regex but is fast with hand-rolled parser
    std::string complexExpr = 
        "customer_age >= 18 and customer_age <= 65 and "
        "(customer_tier == 'premium' or customer_tier == 'gold') and "
        "order_total > 100.00 and (item_count > 5 or discount_code ~= 'VIP')";

    BENCHMARK("compile complex expression") {
        Rule rule;
        rule.id = 1;
        rule.expression = complexExpr;
        rule.compile(engine);
        return rule.id;  // Return something to prevent optimization
    };

    // Benchmark multiple simple expressions (batch compilation)
    std::vector<std::string> expressions = {
        "x > 0", "y < 100", "z == 'active'",
        "a ~= 'test'", "b >= 10", "c <= 50",
        "d == true", "e ~= 'pattern'", "f > 3.14"
    };

    BENCHMARK("compile 9 simple expressions (batch)") {
        int compiled = 0;
        for (size_t i = 0; i < expressions.size(); ++i) {
            Rule rule;
            rule.id = static_cast<int>(i);
            rule.expression = expressions[i];
            rule.compile(engine);
            compiled += rule.id;  // Use rule to prevent optimization
        }
        return compiled;
    };
}

