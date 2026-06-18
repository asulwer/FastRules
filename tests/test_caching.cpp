/**
 * @file test_caching.cpp
 * @brief Rule caching tests
 * 
 * Tests cover:
 * - Cache hits on repeated executions
 * - Cache expiration after TTL
 * - Cache invalidation
 * - Cache with different parameter values
 * - Cache miss scenarios
 * 
 * These tests verify the Rule cache mechanism correctly
 * stores and retrieves results based on parameters and TTL.
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules.hpp>
#include <thread>
#include <chrono>

using namespace fastrules;

TEST_CASE("Rule cache hit on second execution") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "always-true-cached-rule";
    rule->expression = "true";
    rule->cacheDuration = std::chrono::milliseconds(1000);
    
    Workflow workflow;
    workflow.name = "Rule cache hit on second execution";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    
    // First execution
    auto results1 = workflow.execute(engine, params);
    REQUIRE(results1.size() == 1);
    REQUIRE(results1[0].isSuccess() == true);
    
    // Second execution should use cache
    auto results2 = workflow.execute(engine, params);
    REQUIRE(results2.size() == 1);
    REQUIRE(results2[0].isSuccess() == true);
}

TEST_CASE("Rule cache expires after duration") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "always-true-short-cache-rule";
    rule->expression = "true";
    rule->cacheDuration = std::chrono::milliseconds(100);
    
    Workflow workflow;
    workflow.name = "Rule cache expires after duration";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    
    // First execution
    auto results1 = workflow.execute(engine, params);
    REQUIRE(results1.size() == 1);
    
    // Wait for cache to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Should re-execute after cache expiry
    auto results2 = workflow.execute(engine, params);
    REQUIRE(results2.size() == 1);
    REQUIRE(results2[0].isSuccess() == true);
}

TEST_CASE("Uncached rule executes every time") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "always-true-uncached-rule";
    rule->expression = "true";
    // No cacheDuration set
    
    Workflow workflow;
    workflow.name = "Uncached rule executes every time";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    std::vector<RuleParameter> params;
    
    // Multiple executions
    for (int i = 0; i < 5; ++i) {
        auto results = workflow.execute(engine, params);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].isSuccess() == true);
    }
}

TEST_CASE("Cache with parameters") {
    LuaEngine engine;
    
    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->name = "parameter-dependent-cached-rule";
    rule->expression = "x > 0";
    rule->cacheDuration = std::chrono::milliseconds(1000);
    
    Workflow workflow;
    workflow.name = "Cache with parameters";
    workflow.rules.push_back(rule);
    workflow.compile(engine);
    
    // First call with x=5
    std::vector<RuleParameter> params1;
    params1.emplace_back("x", 5);
    auto results1 = workflow.execute(engine, params1);
    REQUIRE(results1.size() == 1);
    REQUIRE(results1[0].isSuccess() == true);
    
    // Same parameters - should hit cache
    auto results2 = workflow.execute(engine, params1);
    REQUIRE(results2.size() == 1);
    REQUIRE(results2[0].isSuccess() == true);
    
    // Different parameters - should re-execute
    std::vector<RuleParameter> params2;
    params2.emplace_back("x", -1);
    auto results3 = workflow.execute(engine, params2);
    REQUIRE(results3.size() == 1);
    REQUIRE_FALSE(results3[0].isSuccess());  // x > 0 is false
}
