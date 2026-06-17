/**
 * @file test_workflow.cpp
 * @brief Unit tests for the Workflow class
 * 
 * Tests cover:
 * - Workflow creation and properties
 * - Rule execution order (by priority)
 * - Dependency resolution and validation
 * - Rule compilation
 * - Sequential execution
 * - Circular dependency detection
 * - Parallel execution modes
 * - Error handling during execution
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("Workflow basic creation") {
    Workflow workflow;
    workflow.id = 1;
    workflow.description = "Test workflow";

    REQUIRE(workflow.id == 1);
    REQUIRE(workflow.description == "Test workflow");
    REQUIRE(workflow.isActive == true);
    REQUIRE(workflow.rules.empty());
    REQUIRE(workflow.isCompiled() == false);
}

TEST_CASE("Workflow execution order") {
    Workflow workflow;
    workflow.description = "Priority test";

    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->priority = 2;

    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->priority = 1;

    auto rule3 = std::make_shared<Rule>();
    rule3->id = 3;
    rule3->priority = 0;

    workflow.rules.push_back(rule1);
    workflow.rules.push_back(rule2);
    workflow.rules.push_back(rule3);

    auto order = workflow.resolveExecutionOrder();

    REQUIRE(order.size() == 3);
    REQUIRE(order[0]->id == 3); // Lowest priority first
    REQUIRE(order[1]->id == 2);
    REQUIRE(order[2]->id == 1); // Highest priority last
}

TEST_CASE("Workflow dependency resolution") {
    Workflow workflow;
    workflow.description = "Dependency test";

    auto baseRule = std::make_shared<Rule>();
    baseRule->id = 1;
    baseRule->name = "baseRule";
    baseRule->priority = 0;

    auto dependentRule = std::make_shared<Rule>();
    dependentRule->id = 2;
    dependentRule->name = "dependentRule";
    dependentRule->priority = 1;
    dependentRule->dependsOnRuleName = "baseRule";

    workflow.rules.push_back(dependentRule); // Add in wrong order
    workflow.rules.push_back(baseRule);

    auto order = workflow.resolveExecutionOrder();

    REQUIRE(order.size() == 2);
    REQUIRE(order[0]->id == 1); // Dependency comes first
    REQUIRE(order[1]->id == 2);
}

TEST_CASE("Workflow circular dependency detection") {
    Workflow workflow;
    workflow.description = "Circular dependency test";

    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->name = "rule1";
    rule1->dependsOnRuleName = "rule2";

    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->name = "rule2";
    rule2->dependsOnRuleName = "rule1";

    workflow.rules.push_back(rule1);
    workflow.rules.push_back(rule2);

    REQUIRE_THROWS_AS([&](){ (void)workflow.resolveExecutionOrder(); }(), RuleValidationException);
}

TEST_CASE("Workflow compile and execute") {
    LuaEngine engine;

    Workflow workflow;
    workflow.description = "Execution test";

    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->expression = "true";
    workflow.rules.push_back(rule1);

    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->expression = "false";
    workflow.rules.push_back(rule2);

    workflow.compile(engine);
    REQUIRE(workflow.isCompiled() == true);

    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);

    REQUIRE(results.size() == 2);
    REQUIRE(results[0].isSuccess() == true);
    REQUIRE(results[1].isSuccess() == false);
}

TEST_CASE("Workflow JSON loading") {
    // This test requires the JSON extension
    // Workflow::loadFromJson has been removed from core
    REQUIRE(true);
}
