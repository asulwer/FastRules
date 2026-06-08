// comprehensive_example.cpp
// Demonstrates using all three persistence extensions together:
// JSON, XML, and database. Shows round-tripping between formats.

#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>
#include <fastrules/xml_loader.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace fastrules;

int main() {
    try {
        LuaEngine engine;

        // ================================================================
        // PHASE 1: Create a workflow from JSON
        // ================================================================
        std::cout << "=== Phase 1: Load workflow from JSON ===\n";

        std::string jsonInput = R"({
            "id": "loan-approval",
            "description": "Loan approval workflow",
            "isActive": true,
            "rules": [
                {
                    "id": "income-check",
                    "expression": "app.annualIncome >= 50000",
                    "action": "incomeOk = true",
                    "isActive": true,
                    "priority": 10,
                    "parameterNames": ["app"]
                },
                {
                    "id": "debt-ratio-check",
                    "expression": "app.debtRatio <= 0.43",
                    "action": "debtOk = true",
                    "isActive": true,
                    "priority": 5,
                    "parameterNames": ["app"],
                    "dependsOnRuleId": "income-check"
                },
                {
                    "id": "credit-score-check",
                    "expression": "app.creditScore >= 650",
                    "action": "creditOk = true",
                    "isActive": true,
                    "priority": 3,
                    "parameterNames": ["app"]
                }
            ]
        })";

        auto workflow = JsonLoader::loadWorkflow(jsonInput);
        std::cout << "Loaded: " << workflow.id << " with " << workflow.rules.size() << " rules\n";

        // ================================================================
        // PHASE 2: Execute the workflow
        // ================================================================
        std::cout << "\n=== Phase 2: Execute workflow ===\n";

        struct LoanApplication {
            double annualIncome = 75000;
            double debtRatio = 0.35;
            int creditScore = 720;
        };

        engine.registerType<LoanApplication>("LoanApplication", [](auto& ut) {
            ut["annualIncome"] = &LoanApplication::annualIncome;
            ut["debtRatio"] = &LoanApplication::debtRatio;
            ut["creditScore"] = &LoanApplication::creditScore;
        });

        LoanApplication app;
        app.annualIncome = 75000;
        app.debtRatio = 0.35;
        app.creditScore = 720;

        std::vector<RuleParameter> params;
        params.emplace_back("app", "LoanApplication", std::any(&app));

        workflow.compile(engine);
        auto results = workflow.execute(engine, params);

        for (const auto& result : results) {
            std::cout << "  Rule " << result.ruleId << ": "
                      << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        for (const auto& result : results) {
            std::cout << "  Rule " << result.ruleId << ": "
                      << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        // ================================================================
        // PHASE 3: Save workflow to XML
        // ================================================================
        std::cout << "\n=== Phase 3: Convert to XML ===\n";

        std::string xmlOutput = XmlLoader::saveWorkflowPretty(workflow);
        std::string xmlFile = "loan-workflow.xml";
        {
            std::ofstream file(xmlFile);
            file << xmlOutput;
        }
        std::cout << "Saved XML to " << xmlFile << "\n";

        // ================================================================
        // PHASE 4: Reload from XML and verify
        // ================================================================
        std::cout << "\n=== Phase 4: Reload from XML ===\n";

        auto xmlWorkflow = XmlLoader::loadWorkflow(xmlOutput);
        std::cout << "Reloaded: " << xmlWorkflow.id << " with " << xmlWorkflow.rules.size() << " rules\n";

        // Verify round-trip integrity
        bool roundTripOk = true;
        for (size_t i = 0; i < workflow.rules.size(); ++i) {
            if (workflow.rules[i]->id != xmlWorkflow.rules[i]->id) {
                roundTripOk = false;
                break;
            }
        }
        std::cout << "Round-trip integrity: " << (roundTripOk ? "OK" : "MISMATCH") << "\n";

        // Execute the XML-loaded workflow
        xmlWorkflow.compile(engine);

        LoanApplication app2;
        app2.annualIncome = 75000;
        app2.debtRatio = 0.35;
        app2.creditScore = 720;

        std::vector<RuleParameter> xmlParams;
        xmlParams.emplace_back("app", "LoanApplication", std::any(&app2));

        auto xmlResults = xmlWorkflow.execute(engine, xmlParams);
        for (const auto& result : xmlResults) {
            std::cout << "  Rule " << result.ruleId << ": "
                      << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        // ================================================================
        // PHASE 5: Serialize individual rules to both formats
        // ================================================================
        std::cout << "\n=== Phase 5: Cross-format rule serialization ===\n";

        for (const auto& rule : workflow.rules) {
            // JSON
            std::string ruleJson = JsonLoader::saveRulePretty(*rule);
            std::cout << "\nRule " << rule->id << " (JSON):\n" << ruleJson << "\n";

            // XML
            std::string ruleXml = XmlLoader::saveRulePretty(*rule);
            std::cout << "Rule " << rule->id << " (XML):\n" << ruleXml << "\n";
        }

        // ================================================================
        // PHASE 6: Compare format sizes
        // ================================================================
        std::cout << "\n=== Phase 6: Format comparison ===\n";

        std::string compactJson = JsonLoader::saveWorkflow(workflow);
        std::string compactXml = XmlLoader::saveWorkflow(workflow);

        std::cout << "JSON size: " << compactJson.size() << " bytes\n";
        std::cout << "XML size:  " << compactXml.size() << " bytes\n";

        // ================================================================
        // PHASE 7: Clean up
        // ================================================================
        std::filesystem::remove(xmlFile);
        std::cout << "\nCleaned up temporary files.\n";

        std::cout << "\n=== Comprehensive example complete ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
