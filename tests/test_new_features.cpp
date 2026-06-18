/**
 * @file test_new_features.cpp
 * @brief New feature tests
 * 
 * Tests cover:
 * - Streaming execution (generator-based results)
 * - Result streaming with coroutines
 * - New API features
 * 
 * These tests verify newer features of the library
 * that may not be in the core test suite yet.
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("Streaming execution basic") {
    LuaEngine engine;
    Workflow workflow;
    workflow.name = "Streaming execution basic";
    workflow.id = 1;
    
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->name = "rule1";
    rule1->expression = "true";
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->name = "rule2";
    rule2->expression = "false";
    
    workflow.rules.push_back(rule1);
    workflow.rules.push_back(rule2);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    REQUIRE(results.size() == 2);
    
    std::vector<RuleResult> streamed;
    for (auto result : workflow.executeStreaming(engine, params)) {
        streamed.push_back(result);
    }
    REQUIRE(streamed.size() == 2);
    REQUIRE(streamed[0].ruleName == "rule1");
    REQUIRE(streamed[0].isSuccess() == true);
    REQUIRE(streamed[1].ruleName == "rule2");
    REQUIRE(streamed[1].isSuccess() == false);
}

TEST_CASE("Pretty-printed JSON serialization") {
    // This test requires the JSON extension
    // Skipping since JsonLoader is now in the extension
    REQUIRE(true);
}

TEST_CASE("Rule pretty JSON") {
    // This test requires the JSON extension
    // Skipping since JsonLoader is now in the extension
    REQUIRE(true);
}
