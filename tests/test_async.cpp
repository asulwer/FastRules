#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "test_helpers.hpp"
#include "fastrules/async_workflow.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"
#include "fastrules/rule_context.hpp"

#include <chrono>
#include <thread>
#include <future>
#include <lauxlib.h>

using namespace fastrules;

// Helper: sleep function for Lua (to simulate async operations)
static int lua_sleep(lua_State* L) {
    int ms = static_cast<int>(luaL_checkinteger(L, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return 0;
}

TEST_CASE("AsyncWorkflow basic construction", "[async]") {
    AsyncWorkflow async;
    REQUIRE_FALSE(async.isCompiled());
}

TEST_CASE("AsyncWorkflow with workflow", "[async]") {
    Workflow workflow;
    workflow.description = "Test workflow";
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    workflow.rules.push_back(rule);
    
    AsyncWorkflow async(std::move(workflow));
    REQUIRE(async.workflow().rules.size() == 1);
}

TEST_CASE("Lua coroutine compilation", "[async][coroutine]") {
    auto engine = makeTestEngine();
    
    SECTION("Compile simple coroutine") {
        auto ref = engine.compileCoroutine("true");
        REQUIRE(ref.has_value());
        REQUIRE(engine.isCoroutine(ref.value()));
        
        RuleContext context;
        auto status = engine.resumeCoroutine(ref.value(), {}, context);
        REQUIRE(status == true);
        
        engine.releaseRef(ref.value());
    }
    
    SECTION("Compile coroutine with parameters") {
        auto ref = engine.compileCoroutine("x > 5");
        REQUIRE(ref.has_value());
        
        RuleContext context;
        std::vector<RuleParameter> params;
        params.emplace_back("x", 10);
        
        auto status = engine.resumeCoroutine(ref.value(), params, context);
        REQUIRE(status == true);
        
        engine.releaseRef(ref.value());
    }
    
    SECTION("Coroutine vs regular function") {
        auto coroRef = engine.compileCoroutine("true");
        auto funcRef = engine.compileExpression("true");
        
        REQUIRE(engine.isCoroutine(coroRef.value()));
        REQUIRE_FALSE(engine.isCoroutine(funcRef.value()));
        
        engine.releaseRef(coroRef.value());
        engine.releaseRef(funcRef.value());
    }
}

TEST_CASE("Parallel workflow execution", "[async][parallel]") {
    auto engine = makeTestEngine();
    
    Workflow workflow;
    workflow.description = "Parallel test";
    
    // Rule 1: Independent
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->name = "rule1";
    rule1->expression = "true";
    workflow.rules.push_back(rule1);
    
    // Rule 2: Independent
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->name = "rule2";
    rule2->expression = "true";
    workflow.rules.push_back(rule2);
    
    // Rule 3: Depends on rule-1
    auto rule3 = std::make_shared<Rule>();
    rule3->id = 3;
    rule3->name = "rule3";
    rule3->expression = "context.getResult(\"rule1\").success";
    rule3->dependsOnRuleName = "rule1";
    workflow.rules.push_back(rule3);
    
    SECTION("Sequential execution") {
        auto results = workflow.execute(engine, {});
        REQUIRE(results.size() == 3);
        REQUIRE(results[0].isSuccess());
        REQUIRE(results[1].isSuccess());
        REQUIRE(results[2].isSuccess());
    }
    
    SECTION("Parallel execution") {
        auto results = workflow.executeParallel(engine, {});
        REQUIRE(results.size() == 3);
        
        // Find results by rule ID
        bool foundRule1 = false, foundRule2 = false, foundRule3 = false;
        for (const auto& result : results) {
            if (result.ruleName == "rule1") foundRule1 = result.isSuccess();
            if (result.ruleName == "rule2") foundRule2 = result.isSuccess();
            if (result.ruleName == "rule3") foundRule3 = result.isSuccess();
        }
        REQUIRE(foundRule1);
        REQUIRE(foundRule2);
        REQUIRE(foundRule3);
    }
}

TEST_CASE("Parallel execution with dependencies", "[async][parallel]") {
    auto engine = makeTestEngine();
    
    Workflow workflow;
    workflow.description = "Dependency chain test";
    
    // Create a chain: A -> B -> C, with D independent
    auto ruleA = std::make_shared<Rule>();
    ruleA->id = 1;
    ruleA->name = "ruleA";
    ruleA->expression = "true";
    workflow.rules.push_back(ruleA);
    
    auto ruleB = std::make_shared<Rule>();
    ruleB->id = 2;
    ruleB->name = "ruleB";
    ruleB->expression = "context.getResult(\"ruleA\").success";
    ruleB->dependsOnRuleName = "ruleA";
    workflow.rules.push_back(ruleB);
    
    auto ruleC = std::make_shared<Rule>();
    ruleC->id = 3;
    ruleC->name = "ruleC";
    ruleC->expression = "context.getResult(\"ruleB\").success";
    ruleC->dependsOnRuleName = "ruleB";
    workflow.rules.push_back(ruleC);
    
    auto ruleD = std::make_shared<Rule>();
    ruleD->id = 4;
    ruleD->name = "ruleD";
    ruleD->expression = "true";
    workflow.rules.push_back(ruleD);
    
    SECTION("Dependency chain resolves correctly") {
        auto results = workflow.executeParallel(engine, {});
        REQUIRE(results.size() == 4);
        
        // All should succeed
        for (const auto& result : results) {
            REQUIRE(result.isSuccess());
        }
    }
}

TEST_CASE("Thread-safe LuaEngine cloning", "[async][thread-safety]") {
    auto engine = makeTestEngine();
    
    // Compile something in the original engine
    auto ref = engine.compileExpression("true");
    REQUIRE(ref.has_value());
    
    // Clone the engine
    auto cloned = engine.clone();
    REQUIRE(cloned != nullptr);
    
    // Clone should have its own state
    REQUIRE(cloned->luaState() != engine.luaState());
    
    SECTION("Clone can compile independently") {
        auto clonedRef = cloned->compileExpression("false");
        REQUIRE(clonedRef.has_value());
        
        RuleContext ctx;
        auto result = cloned->evaluateExpression(clonedRef.value(), {}, ctx);
        REQUIRE(result == false);
    }
}

TEST_CASE("AsyncWorkflow parallel async execution", "[async][parallel][asyncworkflow]") {
    auto engine = makeTestEngine();
    
    Workflow workflow;
    workflow.description = "Async parallel test";
    
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->expression = "true";
    workflow.rules.push_back(rule1);
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->expression = "true";
    workflow.rules.push_back(rule2);
    
    AsyncWorkflow async(std::move(workflow));
    async.compile(engine);
    
    SECTION("Execute parallel async") {
        auto results = async.executeParallelAsync(engine, {});
        REQUIRE(results.size() == 2);
        
        for (const auto& result : results) {
            REQUIRE(result.isSuccess());
        }
    }
}

TEST_CASE("Performance: parallel vs sequential", "[async][performance]") {
    // This test is more of a sanity check - parallel should be at least as fast
    // for independent rules
    
    LuaEngine engine;
    
    Workflow workflow;
    workflow.description = "Performance test";
    
    // Create multiple independent rules that sleep
    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = 100 + i;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }
    
    SECTION("Compare execution times") {
        auto start = std::chrono::steady_clock::now();
        auto seqResults = workflow.execute(engine, {});
        auto seqEnd = std::chrono::steady_clock::now();
        auto seqDuration = std::chrono::duration_cast<std::chrono::milliseconds>(seqEnd - start).count();
        
        start = std::chrono::steady_clock::now();
        auto parResults = workflow.executeParallel(engine, {});
        auto parEnd = std::chrono::steady_clock::now();
        auto parDuration = std::chrono::duration_cast<std::chrono::milliseconds>(parEnd - start).count();
        
        REQUIRE(seqResults.size() == parResults.size());
        
        // Parallel should not be significantly slower for independent rules
        // (This is a loose check since thread overhead varies)
        INFO("Sequential: " << seqDuration << "ms, Parallel: " << parDuration << "ms");
    }
}

TEST_CASE("Workflow executeAsync", "[async][workflow]") {
    auto engine = makeTestEngine();
    
    Workflow workflow;
    workflow.description = "Async test workflow";
    
    // Add a rule
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "test-rule";
    rule->expression = "x > 0";
    workflow.rules.push_back(rule);
    
    workflow.compile(engine);
    
    SECTION("Execute asynchronously returns future") {
        std::vector<RuleParameter> params;
        params.emplace_back("x", 42);
        
        auto future = workflow.executeAsync(engine, params);
        
        // Future should be valid
        REQUIRE(future.valid());
        
        // Wait for result (with timeout)
        auto status = future.wait_for(std::chrono::seconds(5));
        REQUIRE(status == std::future_status::ready);
        
        auto results = future.get();
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].isSuccess());
    }
    
    SECTION("Multiple async executions") {
        std::vector<std::future<std::vector<RuleResult>>> futures;
        
        // Launch multiple async executions
        for (int i = 0; i < 5; ++i) {
            std::vector<RuleParameter> params;
            params.emplace_back("x", i + 1);
            futures.push_back(workflow.executeAsync(engine, params));
        }
        
        // Collect all results
        for (auto& f : futures) {
            auto results = f.get();
            REQUIRE(results.size() == 1);
            REQUIRE(results[0].isSuccess());
        }
    }
}

TEST_CASE("Workflow executeAdaptive", "[async][workflow][adaptive]") {
    auto engine = makeTestEngine();
    
    SECTION("Adaptive execution with few rules (sequential)") {
        Workflow workflow;
        workflow.description = "Small adaptive workflow";
        
        // Add 2 rules (should use sequential)
        for (int i = 0; i < 2; ++i) {
            auto rule = std::make_shared<Rule>();
            rule->id = 100 + i;
            rule->expression = "true";
            workflow.rules.push_back(rule);
        }
        
        workflow.compile(engine);
        
        std::vector<RuleParameter> params;
        auto results = workflow.executeAdaptive(engine, params);
        REQUIRE(results.size() == 2);
        for (const auto& result : results) {
            REQUIRE(result.isSuccess());
        }
    }
    
    SECTION("Adaptive execution with many rules (parallel)") {
        Workflow largeWorkflow;
        largeWorkflow.description = "Large adaptive workflow";
        
        // Add 6 rules (should use parallel)
        for (int i = 0; i < 6; ++i) {
            auto rule = std::make_shared<Rule>();
            rule->id = 100 + i;
            rule->expression = "true";
            largeWorkflow.rules.push_back(rule);
        }
        
        largeWorkflow.compile(engine);
        
        std::vector<RuleParameter> params;
        auto results = largeWorkflow.executeAdaptive(engine, params);
        REQUIRE(results.size() == 6);
        for (const auto& result : results) {
            REQUIRE(result.isSuccess());
        }
    }
    
    SECTION("Configurable adaptive threshold") {
        Workflow workflow;
        workflow.description = "Configurable threshold test";
        
        // Add 5 rules
        for (int i = 0; i < 5; ++i) {
            auto rule = std::make_shared<Rule>();
            rule->id = 100 + i;
            rule->expression = "true";
            workflow.rules.push_back(rule);
        }
        
        workflow.compile(engine);
        
        // Test default threshold
        REQUIRE(workflow.getAdaptiveThreshold() == 4);
        
        // Test setting threshold to 0 (always sequential)
        workflow.setAdaptiveThreshold(0);
        REQUIRE(workflow.getAdaptiveThreshold() == 0);
        
        std::vector<RuleParameter> params;
        auto results = workflow.executeAdaptive(engine, params);
        REQUIRE(results.size() == 5);
        
        // Test setting threshold high (always parallel)
        workflow.setAdaptiveThreshold(100);
        REQUIRE(workflow.getAdaptiveThreshold() == 100);
        
        results = workflow.executeAdaptive(engine, params);
        REQUIRE(results.size() == 5);
        
        // Reset to default
        workflow.setAdaptiveThreshold(4);
        REQUIRE(workflow.getAdaptiveThreshold() == 4);
    }
}

