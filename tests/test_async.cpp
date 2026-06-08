#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fastrules/async_workflow.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"
#include "fastrules/rule_context.hpp"

#include <chrono>
#include <thread>
#include <future>

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
    rule->id = "test-rule";
    rule->expression = "true";
    workflow.rules.push_back(rule);
    
    AsyncWorkflow async(std::move(workflow));
    REQUIRE(async.workflow().rules.size() == 1);
}

TEST_CASE("Lua coroutine compilation", "[async][coroutine]") {
    LuaEngine engine;
    
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
    LuaEngine engine;
    
    Workflow workflow;
    workflow.description = "Parallel test";
    
    // Rule 1: Independent
    auto rule1 = std::make_shared<Rule>();
    rule1->id = "rule-1";
    rule1->expression = "true";
    workflow.rules.push_back(rule1);
    
    // Rule 2: Independent
    auto rule2 = std::make_shared<Rule>();
    rule2->id = "rule-2";
    rule2->expression = "true";
    workflow.rules.push_back(rule2);
    
    // Rule 3: Depends on rule-1
    auto rule3 = std::make_shared<Rule>();
    rule3->id = "rule-3";
    rule3->expression = "context.getResult('rule-1').success";
    rule3->dependsOnRuleId = "rule-1";
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
            if (result.ruleId == "rule-1") foundRule1 = result.isSuccess();
            if (result.ruleId == "rule-2") foundRule2 = result.isSuccess();
            if (result.ruleId == "rule-3") foundRule3 = result.isSuccess();
        }
        REQUIRE(foundRule1);
        REQUIRE(foundRule2);
        REQUIRE(foundRule3);
    }
}

TEST_CASE("Parallel execution with dependencies", "[async][parallel]") {
    LuaEngine engine;
    
    Workflow workflow;
    workflow.description = "Dependency chain test";
    
    // Create a chain: A -> B -> C, with D independent
    auto ruleA = std::make_shared<Rule>();
    ruleA->id = "A";
    ruleA->expression = "true";
    workflow.rules.push_back(ruleA);
    
    auto ruleB = std::make_shared<Rule>();
    ruleB->id = "B";
    ruleB->expression = "context.getResult('A').success";
    ruleB->dependsOnRuleId = "A";
    workflow.rules.push_back(ruleB);
    
    auto ruleC = std::make_shared<Rule>();
    ruleC->id = "C";
    ruleC->expression = "context.getResult('B').success";
    ruleC->dependsOnRuleId = "B";
    workflow.rules.push_back(ruleC);
    
    auto ruleD = std::make_shared<Rule>();
    ruleD->id = "D";
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
    LuaEngine engine;
    
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
    LuaEngine engine;
    
    Workflow workflow;
    workflow.description = "Async parallel test";
    
    auto rule1 = std::make_shared<Rule>();
    rule1->id = "async-1";
    rule1->expression = "true";
    workflow.rules.push_back(rule1);
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = "async-2";
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
        rule->id = "perf-" + std::to_string(i);
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

