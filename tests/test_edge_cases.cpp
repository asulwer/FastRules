/**
 * @file test_edge_cases.cpp
 * @brief Edge case and boundary condition tests
 * 
 * Tests cover:
 * - Invalid Lua expression syntax
 * - Undefined variables
 * - Empty workflows and rules
 * - Deeply nested expressions
 * - Large data structures
 * - Boundary values (min/max int, empty strings)
 * - Unicode and special characters
 * - Race conditions
 * 
 * These tests verify the engine handles edge cases
 * gracefully without crashing or undefined behavior.
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules.hpp>
#include <thread>
#include <atomic>

using namespace fastrules;

// ============================================================================
// Invalid Lua Expressions
// ============================================================================

TEST_CASE("Invalid Lua expression syntax error") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "if then else end";
    
    Workflow workflow;
    workflow.name = "Invalid Lua expression syntax error";
    workflow.rules.push_back(rule);
    
    REQUIRE_THROWS_AS(workflow.compile(engine), RuleCompilationException);
}

TEST_CASE("Invalid Lua expression undefined variable") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "undefined_variable > 0";
    rule->timeout = std::chrono::milliseconds(1000);  // Add timeout to prevent hanging
    
    Workflow workflow;
    workflow.name = "Invalid Lua expression undefined variable";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 1);
    REQUIRE_FALSE(results[0].isSuccess());
}

TEST_CASE("Invalid Lua expression malformed function call") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "math.(";
    
    Workflow workflow;
    workflow.name = "Invalid Lua expression malformed function call";
    workflow.rules.push_back(rule);
    
    REQUIRE_THROWS_AS(workflow.compile(engine), RuleCompilationException);
}

// ============================================================================
// Timeout Enforcement
// ============================================================================

TEST_CASE("Rule timeout with action") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->action = "while true do end";
    rule->timeout = std::chrono::milliseconds(100);
    
    Workflow workflow;
    workflow.name = "Rule timeout with action";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 1);
    REQUIRE_FALSE(results[0].isSuccess());
}

// ============================================================================
// Circular Dependencies
// ============================================================================

TEST_CASE("Circular dependency detection direct") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Circular dependency detection direct";
    
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->name = "rule1";
    rule1->expression = "true";
    rule1->dependsOnRuleName = "rule2";
    workflow.rules.push_back(rule1);
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->name = "rule2";
    rule2->expression = "true";
    rule2->dependsOnRuleName = "rule1";
    workflow.rules.push_back(rule2);
    
    REQUIRE_THROWS_AS(workflow.validate(), RuleValidationException);
}

TEST_CASE("Circular dependency detection indirect") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Circular dependency detection indirect";
    
    auto ruleA = std::make_shared<Rule>();
    ruleA->id = 1;
    ruleA->name = "ruleA";
    ruleA->expression = "true";
    ruleA->dependsOnRuleName = "ruleB";
    workflow.rules.push_back(ruleA);
    
    auto ruleB = std::make_shared<Rule>();
    ruleB->id = 2;
    ruleB->name = "ruleB";
    ruleB->expression = "true";
    ruleB->dependsOnRuleName = "ruleC";
    workflow.rules.push_back(ruleB);
    
    auto ruleC = std::make_shared<Rule>();
    ruleC->id = 3;
    ruleC->name = "ruleC";
    ruleC->expression = "true";
    ruleC->dependsOnRuleName = "ruleA";
    workflow.rules.push_back(ruleC);
    
    REQUIRE_THROWS_AS(workflow.validate(), RuleValidationException);
}

TEST_CASE("Circular dependency self-reference") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Circular dependency self-reference";
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "self_ref";
    rule->expression = "true";
    rule->dependsOnRuleName = "self_ref";
    workflow.rules.push_back(rule);
    
    REQUIRE_THROWS_AS(workflow.validate(), RuleValidationException);
}

// ============================================================================
// Maximum Recursion Depth
// ============================================================================

TEST_CASE("Deeply nested rule dependencies") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Deeply nested rule dependencies";
    
    for (int i = 0; i < 50; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i + 1;
        rule->name = "rule" + std::to_string(i + 1);
        rule->expression = "true";
        if (i > 0) {
            rule->dependsOnRuleName = "rule" + std::to_string(i);
        }
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 50);
}

// ============================================================================
// Concurrent Modifications
// ============================================================================

TEST_CASE("Concurrent workflow execution thread safety") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Concurrent workflow execution thread safety";
    workflow.id = 1;
    
    for (int i = 0; i < 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i + 1;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&]() {
            std::vector<RuleParameter> params;
            for (int i = 0; i < 100; ++i) {
                auto results = workflow.execute(engine, params);
                if (results.size() == 10) {
                    successCount++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    REQUIRE(successCount == 1000);
}

TEST_CASE("Concurrent parallel execution thread safety") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Concurrent parallel execution thread safety";
    workflow.id = 1;
    
    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i + 1;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([&]() {
            std::vector<RuleParameter> params;
            for (int i = 0; i < 20; ++i) {
                auto results = workflow.executeParallel(engine, params);
                if (results.size() == 5) {
                    successCount++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    REQUIRE(successCount == 100);
}

// ============================================================================
// Additional Edge Cases
// ============================================================================

TEST_CASE("Duplicate rule IDs detection") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Duplicate rule IDs detection";
    
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->expression = "true";
    workflow.rules.push_back(rule1);
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 1;
    rule2->expression = "true";
    workflow.rules.push_back(rule2);
    
    REQUIRE_THROWS_AS(workflow.validate(), RuleValidationException);
}

TEST_CASE("Missing dependency detection") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Missing dependency detection";
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->dependsOnRuleName = "non_existent_rule";
    workflow.rules.push_back(rule);
    
    // Should throw during compile/validate because dependency doesn't exist
    REQUIRE_THROWS_AS(workflow.compile(engine), RuleValidationException);
}

TEST_CASE("Workflow with only inactive rules") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Workflow with only inactive rules";
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->isActive = false;
    workflow.rules.push_back(rule);
    
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.empty());
}

TEST_CASE("Workflow with all failing rules") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Workflow with all failing rules";
    
    for (int i = 0; i < 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i + 1;
        rule->expression = "false";
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 5);
    for (const auto& result : results) {
        REQUIRE_FALSE(result.isSuccess());
    }
}

TEST_CASE("Very long expression handling") {
    LuaEngine engine;
    
    std::string longExpr;
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) longExpr += " and ";
        longExpr += "(" + std::to_string(i) + " > " + std::to_string(i-1) + ")";
    }
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = longExpr;
    
    Workflow workflow;
    workflow.name = "Very long expression handling";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 1);
}

TEST_CASE("Unicode in rule names and descriptions") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Unicode in rule names and descriptions";
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "规则一";
    rule->description = "Règle de test ✨";
    rule->expression = "true";
    workflow.rules.push_back(rule);
    
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].ruleName == "规则一");
}

TEST_CASE("Empty workflow execution") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Empty workflow execution";
    
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.empty());
}

TEST_CASE("Single rule workflow") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Single rule workflow";
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "rule1";
    rule->expression = "true";
    workflow.rules.push_back(rule);
    
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].ruleName == "rule1");
}

TEST_CASE("Rule with empty expression") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "";
    
    Workflow workflow;
    workflow.name = "Rule with empty expression";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].isSuccess() == true);
}
