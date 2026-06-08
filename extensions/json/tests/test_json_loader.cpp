#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>

using namespace fastrules;

TEST_CASE("JsonLoader workflow parsing", "[json]") {
    std::string json = R"({
        "id": "test-workflow",
        "description": "Test workflow",
        "isActive": true,
        "rules": [
            {
                "id": "rule1",
                "description": "Rule 1",
                "expression": "true",
                "action": "x = 1",
                "isActive": true,
                "priority": 0,
                "dependsOnRuleId": null,
                "childRules": [],
                "timeout": null,
                "cacheDuration": null
            }
        ]
    })";

    auto workflow = fastrules::JsonLoader::loadWorkflow(json);

    REQUIRE(workflow.id == "test-workflow");
    REQUIRE(workflow.description == "Test workflow");
    REQUIRE(workflow.isActive == true);
    REQUIRE(workflow.rules.size() == 1);
    REQUIRE(workflow.rules[0]->id == "rule1");
    REQUIRE(workflow.rules[0]->expression == "true");
    REQUIRE(workflow.rules[0]->action == "x = 1");
}

TEST_CASE("JsonLoader rule parsing", "[json]") {
    std::string json = R"({
        "id": "test-rule",
        "description": "Test",
        "expression": "value > 0",
        "isActive": true,
        "priority": 5,
        "dependsOnRuleId": "parent-rule",
        "timeout": 1000,
        "cacheDuration": "5000ms"
    })";

    auto rule = fastrules::JsonLoader::loadRule(json);

    REQUIRE(rule->id == "test-rule");
    REQUIRE(rule->priority == 5);
    REQUIRE(rule->dependsOnRuleId.has_value());
    REQUIRE(rule->dependsOnRuleId.value() == "parent-rule");
    REQUIRE(rule->timeout.has_value());
    REQUIRE(rule->timeout->count() == 1000);
    REQUIRE(rule->cacheDuration.has_value());
    REQUIRE(rule->cacheDuration->count() == 5000);
}

TEST_CASE("JsonLoader child rules", "[json]") {
    std::string json = R"({
        "id": "parent",
        "expression": "true",
        "childRules": [
            {
                "id": "child1",
                "expression": "true"
            },
            {
                "id": "child2",
                "expression": "false"
            }
        ]
    })";

    auto rule = fastrules::JsonLoader::loadRule(json);

    REQUIRE(rule->childRules.size() == 2);
    REQUIRE(rule->childRules[0]->id == "child1");
    REQUIRE(rule->childRules[1]->id == "child2");
}

TEST_CASE("JsonLoader auto-generated IDs", "[json]") {
    std::string json = R"({
        "description": "No ID test",
        "rules": [
            {
                "expression": "true"
            }
        ]
    })";

    auto workflow = fastrules::JsonLoader::loadWorkflow(json);

    REQUIRE_FALSE(workflow.id.empty());
    REQUIRE_FALSE(workflow.rules[0]->id.empty());
}

TEST_CASE("JsonLoader workflow serialization", "[json]") {
    Workflow workflow;
    workflow.id = "serialize-test";
    workflow.description = "Serialization test";

    auto rule = std::make_shared<Rule>();
    rule->id = "rule1";
    rule->description = "Test rule";
    rule->expression = "true";
    rule->priority = 1;
    workflow.rules.push_back(rule);

    std::string json = fastrules::JsonLoader::saveWorkflow(workflow);

    REQUIRE(json.find("serialize-test") != std::string::npos);
    REQUIRE(json.find("Test rule") != std::string::npos);
    REQUIRE(json.find("true") != std::string::npos);
}
