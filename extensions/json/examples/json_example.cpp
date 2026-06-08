// json_example.cpp
// Demonstrates the fastrules-json extension: loading workflows from JSON,
// serializing rules, and using the JSON repository for persistence.

#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>
#include <fastrules/json_serialization.hpp>
#include <fastrules/json_repository.hpp>
#include <iostream>
#include <fstream>

using namespace fastrules;
using namespace fastrules::ext;

int main() {
    try {
        LuaEngine engine;

        // ================================================================
        // 1. Load a workflow from JSON string
        // ================================================================
        std::string workflowJson = R"({
            "id": "customer-validation",
            "description": "Validate customer eligibility",
            "isActive": true,
            "rules": [
                {
                    "id": "adult-check",
                    "description": "Check if customer is an adult",
                    "expression": "age >= 18",
                    "action": "isAdult = true",
                    "isActive": true,
                    "priority": 10,
                    "timeout": 100
                },
                {
                    "id": "credit-check",
                    "description": "Check credit score",
                    "expression": "creditScore >= 650",
                    "action": "isCreditWorthy = true",
                    "isActive": true,
                    "priority": 5,
                    "timeout": 200,
                    "dependencyChain": ["adult-check"]
                }
            ]
        })";

        auto workflow = JsonLoader::loadWorkflow(workflowJson);
        std::cout << "Loaded workflow: " << workflow.id << "\n";
        std::cout << "  Rules: " << workflow.rules.size() << "\n";

        // ================================================================
        // 2. Execute the workflow
        // ================================================================
        struct Customer { int age; int creditScore; };
        engine.registerType<Customer>("Customer", [](auto& ut) {
            ut["age"] = &Customer::age;
            ut["creditScore"] = &Customer::creditScore;
        });

        Customer customer{25, 720};
        std::vector<RuleParameter> params;
        params.emplace_back("age", "int", std::any(&customer.age));
        params.emplace_back("creditScore", "int", std::any(&customer.creditScore));

        auto results = workflow.execute(engine, params);
        for (const auto& result : results) {
            std::cout << "  Rule " << result.ruleId << ": " << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        // ================================================================
        // 3. Serialize workflow back to JSON
        // ================================================================
        std::string prettyJson = JsonLoader::saveWorkflowPretty(workflow);
        std::cout << "\nSerialized workflow (pretty):\n" << prettyJson << "\n";

        // ================================================================
        // 4. JSON repository persistence
        // ================================================================
        JsonRuleRepository repo("rules.json");
        for (const auto& rule : workflow.rules) {
            repo.save(*rule);
        }
        repo.flush();
        std::cout << "\nSaved " << repo.count() << " rules to rules.json\n";

        // Reload from file
        auto allRules = repo.findAll();
        std::cout << "Loaded " << allRules.size() << " rules from repository\n";

        // ================================================================
        // 5. Version history serialization
        // ================================================================
        RuleVersionHistory history;
        history.ruleId = "adult-check";
        
        RuleVersion v1;
        v1.versionId = "v1";
        v1.expression = "age >= 21";
        v1.author = "Alice";
        history.addVersion(v1);
        
        RuleVersion v2;
        v2.versionId = "v2";
        v2.expression = "age >= 18";
        v2.author = "Bob";
        v2.parentVersionId = "v1";
        history.addVersion(v2);

        std::string historyJson = JsonSerialization::serialize(history);
        std::cout << "\nVersion history JSON:\n" << historyJson << "\n";

        std::cout << "\nJSON extension example complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
