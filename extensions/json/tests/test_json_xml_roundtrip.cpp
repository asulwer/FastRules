#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>
#include <fastrules/xml_loader.hpp>

using namespace fastrules;

TEST_CASE("JSON to XML roundtrip", "[json][xml][integration]") {
    std::string json = R"({
        "id": "roundtrip-workflow",
        "description": "Test roundtrip",
        "isActive": true,
        "rules": [
            {
                "id": 1,
                "description": "Rule 1",
                "expression": "x > 0",
                "action": "result = x * 2",
                "isActive": true,
                "priority": 5,
                "dependsOnRuleId": null,
                "childRules": [],
                "timeout": 1000,
                "cacheDuration": "5000ms"
            },
            {
                "id": 2,
                "description": "Rule 2",
                "expression": "y == 'hello'",
                "action": "flag = true",
                "isActive": false,
                "priority": 1,
                "dependsOnRuleId": 1,
                "childRules": [],
                "timeout": null,
                "cacheDuration": null
            }
        ]
    })";

    // 1. Load from JSON
    auto workflow = JsonLoader::loadWorkflow(json);
    REQUIRE(workflow.id == "roundtrip-workflow");
    REQUIRE(workflow.rules.size() == 2);
    REQUIRE(workflow.rules[0]->id == 1);
    REQUIRE(workflow.rules[0]->expression == "x > 0");
    REQUIRE(workflow.rules[0]->action == "result = x * 2");
    REQUIRE(workflow.rules[0]->priority == 5);
    REQUIRE(workflow.rules[0]->timeout.has_value());
    REQUIRE(workflow.rules[0]->timeout->count() == 1000);
    REQUIRE(workflow.rules[0]->cacheDuration.has_value());
    REQUIRE(workflow.rules[0]->cacheDuration->count() == 5000);

    REQUIRE(workflow.rules[1]->id == 2);
    REQUIRE(workflow.rules[1]->dependsOnRuleId.has_value());
    REQUIRE(workflow.rules[1]->dependsOnRuleId.value() == 1);
    REQUIRE(workflow.rules[1]->isActive == false);

    // 2. Save to XML
    std::string xml = XmlLoader::saveWorkflow(workflow);
    REQUIRE(xml.find("roundtrip-workflow") != std::string::npos);
    REQUIRE(xml.find("rule1") != std::string::npos);
    // XML escapes > to &gt;, so check for the escaped form
    REQUIRE(xml.find("x &gt; 0") != std::string::npos);

    // 3. Reload from XML
    auto restored = XmlLoader::loadWorkflow(xml);
    REQUIRE(restored.id == workflow.id);
    REQUIRE(restored.description == workflow.description);
    REQUIRE(restored.isActive == workflow.isActive);
    REQUIRE(restored.rules.size() == workflow.rules.size());

    REQUIRE(restored.rules[0]->id == workflow.rules[0]->id);
    REQUIRE(restored.rules[0]->expression == workflow.rules[0]->expression);
    REQUIRE(restored.rules[0]->action == workflow.rules[0]->action);
    REQUIRE(restored.rules[0]->priority == workflow.rules[0]->priority);
    REQUIRE(restored.rules[0]->isActive == workflow.rules[0]->isActive);
    if (workflow.rules[0]->timeout.has_value()) {
        REQUIRE(restored.rules[0]->timeout.has_value());
        REQUIRE(restored.rules[0]->timeout->count() == workflow.rules[0]->timeout->count());
    }
    if (workflow.rules[0]->cacheDuration.has_value()) {
        REQUIRE(restored.rules[0]->cacheDuration.has_value());
        REQUIRE(restored.rules[0]->cacheDuration->count() == workflow.rules[0]->cacheDuration->count());
    }

    REQUIRE(restored.rules[1]->id == workflow.rules[1]->id);
    if (workflow.rules[1]->dependsOnRuleId.has_value()) {
        REQUIRE(restored.rules[1]->dependsOnRuleId.has_value());
        REQUIRE(restored.rules[1]->dependsOnRuleId.value() == workflow.rules[1]->dependsOnRuleId.value());
    }
}
