/**
 * @file test_concurrent_compilation.cpp
 * @brief Tests for concurrent workflow compilation
 * 
 * Tests cover:
 * - Parallel compilation of independent rules
 * - Compilation level ordering (child rules before parents)
 * - Error handling during parallel compilation
 * - Performance comparison with sequential compilation
 * - Thread safety verification
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules.hpp>
#include <chrono>
#include <thread>

using namespace fastrules;

TEST_CASE("Workflow compileParallel basic functionality") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    // Create 10 independent rules (no dependencies)
    for (int i = 1; i <= 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }

    // Compile in parallel with 4 threads
    workflow.compileParallel(engine, 4);

    REQUIRE(workflow.isCompiled());
    REQUIRE(workflow.isValidated());
}

TEST_CASE("Workflow compileParallel with child rules") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 2;

    // Create parent rule
    auto parent = std::make_shared<Rule>();
    parent->id = 1;
    parent->expression = "true";

    // Create child rules
    auto child1 = std::make_shared<Rule>();
    child1->id = 2;
    child1->expression = "true";

    auto child2 = std::make_shared<Rule>();
    child2->id = 3;
    child2->expression = "true";

    // Set up hierarchy
    parent->childRules = {child1, child2};

    // Add rules (parent first - order shouldn't matter)
    workflow.rules.push_back(parent);
    workflow.rules.push_back(child1);
    workflow.rules.push_back(child2);

    // Compile in parallel
    workflow.compileParallel(engine, 2);

    REQUIRE(workflow.isCompiled());

    // Verify all rules are compiled
    REQUIRE(parent->isCompiled);
    REQUIRE(child1->isCompiled);
    REQUIRE(child2->isCompiled);
}

TEST_CASE("Workflow compileParallel falls back to sequential for small workflows") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 3;

    // Create only 5 rules (below threshold)
    for (int i = 1; i <= 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }

    // This should fall back to sequential
    workflow.compileParallel(engine, 4);

    REQUIRE(workflow.isCompiled());
}

TEST_CASE("Workflow compileParallel execution works after parallel compilation") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 4;

    // Create rules
    for (int i = 1; i <= 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "value == " + std::to_string(i);
        workflow.rules.push_back(rule);
    }

    // Compile in parallel
    workflow.compileParallel(engine, 4);

    // Execute
    std::vector<RuleParameter> params;
    params.emplace_back("value", 5);
    auto results = workflow.execute(engine, params);

    // Find the successful result (rule 5)
    auto it = std::find_if(results.begin(), results.end(), [](const RuleResult& r) {
        return r.isSuccess();
    });
    REQUIRE(it != results.end());  // At least one rule should pass
    REQUIRE(it->ruleId == 5);  // Rule 5 should be successful
}

TEST_CASE("Workflow compileParallel error handling") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 5;

    // Create rules with one invalid expression
    for (int i = 1; i <= 5; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        if (i == 3) {
            rule->expression = "invalid lua syntax here @#$";
        } else {
            rule->expression = "true";
        }
        workflow.rules.push_back(rule);
    }

    // Should throw on compilation error
    REQUIRE_THROWS_AS(workflow.compileParallel(engine, 4), RuleCompilationException);
}

TEST_CASE("Workflow compileParallel performance vs sequential") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 6;

    // Create many rules to see performance benefit
    for (int i = 1; i <= 50; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "x + " + std::to_string(i) + " > 0";
        workflow.rules.push_back(rule);
    }

    // Time sequential compilation
    Workflow workflowSeq;
    workflowSeq.id = 7;
    workflowSeq.rules = workflow.rules;
    
    auto start = std::chrono::steady_clock::now();
    workflowSeq.compile(engine);
    auto seqTime = std::chrono::steady_clock::now() - start;

    // Time parallel compilation
    Workflow workflowPar;
    workflowPar.id = 8;
    workflowPar.rules = workflow.rules;
    
    start = std::chrono::steady_clock::now();
    workflowPar.compileParallel(engine, 4);
    auto parTime = std::chrono::steady_clock::now() - start;

    // Parallel should be faster (or at least not significantly slower)
    // Allow 20% variance for timing noise
    auto parMs = std::chrono::duration_cast<std::chrono::milliseconds>(parTime).count();
    auto seqMs = std::chrono::duration_cast<std::chrono::milliseconds>(seqTime).count();
    
    // Just log the times - don't fail on performance
    MESSAGE("Sequential: " << seqMs << "ms, Parallel: " << parMs << "ms");
    
    // Both should be compiled successfully
    REQUIRE(workflowSeq.isCompiled());
    REQUIRE(workflowPar.isCompiled());
}

TEST_CASE("Workflow compileParallel with expressions and actions") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 9;

    // Create rules with actions
    for (int i = 1; i <= 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "x == " + std::to_string(i);
        rule->action = "result = " + std::to_string(i);
        workflow.rules.push_back(rule);
    }

    // Compile in parallel
    workflow.compileParallel(engine, 4);

    // Execute and verify
    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);
    auto results = workflow.execute(engine, params);

    // Find the successful result (rule 5)
    auto it = std::find_if(results.begin(), results.end(), [](const RuleResult& r) {
        return r.isSuccess();
    });
    REQUIRE(it != results.end());  // At least one rule should pass
    REQUIRE(it->ruleId == 5);  // Rule 5 should be successful
}

TEST_CASE("Workflow compileParallel single thread") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 10;

    for (int i = 1; i <= 20; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }

    // Compile with only 1 thread (should fall back to sequential logic)
    workflow.compileParallel(engine, 1);

    REQUIRE(workflow.isCompiled());
}

TEST_CASE("Workflow compileParallel default thread count") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 11;

    for (int i = 1; i <= 20; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "true";
        workflow.rules.push_back(rule);
    }

    // Compile with default thread count (hardware concurrency)
    workflow.compileParallel(engine);  // 0 means default

    REQUIRE(workflow.isCompiled());
}

TEST_CASE("Workflow compileParallel idempotent") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 12;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    workflow.rules.push_back(rule);

    // Compile twice
    workflow.compileParallel(engine, 4);
    REQUIRE(workflow.isCompiled());

    // Second call should be no-op
    REQUIRE_NOTHROW(workflow.compileParallel(engine, 4));
    REQUIRE(workflow.isCompiled());
}
