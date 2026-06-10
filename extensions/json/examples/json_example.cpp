// json_example.cpp
// Demonstrates the fastrules-json extension: loading workflows from JSON files,
// serializing rules, and using the JSON repository for persistence.

#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>
#include <fastrules/json_serialization.hpp>
#include <fastrules/json_repository.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace fastrules;
using namespace fastrules::ext;

static std::string resolveDataPath(const std::string& filename) {
    auto exePath = std::filesystem::current_path();
    // Walk up looking for data/json directory
    for (int i = 0; i < 6; ++i) {
        auto p = exePath / "data" / "json" / filename;
        if (std::filesystem::exists(p)) return p.string();
        auto parent = exePath.parent_path();
        if (parent == exePath) break;
        exePath = parent;
    }
    // Fallback: try current dir directly
    auto fallback = std::filesystem::current_path() / filename;
    return fallback.string();
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open: " + path);
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

int main() {
    try {
        LuaEngine engine;

        // ================================================================
        // 1. Load a workflow from a JSON data file
        // ================================================================
        std::string jsonPath = resolveDataPath("customer_rules.json");
        std::string workflowJson = readFile(jsonPath);

        auto workflow = JsonLoader::loadWorkflow(workflowJson);
        std::cout << "Loaded workflow: " << workflow.id << "\n";
        std::cout << "  Description: " << workflow.description << "\n";
        std::cout << "  Rules: " << workflow.rules.size() << "\n";

        // ================================================================
        // 2. Execute the workflow
        // ================================================================
        struct Customer { int age; std::string name; bool isActive; };
        engine.registerType<Customer>("Customer", [](auto& reg) {
            reg.bind("age", &Customer::age);
            reg.bind("name", &Customer::name);
            reg.bind("isActive", &Customer::isActive);
        });

        Customer customer{25, "John Doe", true};
        std::vector<RuleParameter> params;
        params.emplace_back("customer", &customer);

        workflow.compile(engine);
        auto results = workflow.execute(engine, params);
        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleId << ": "
                      << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        // ================================================================
        // 3. Load dependency workflow from JSON
        // ================================================================
        std::string depPath = resolveDataPath("dependency_rules.json");
        std::string depJson = readFile(depPath);
        auto depWorkflow = JsonLoader::loadWorkflow(depJson);
        std::cout << "\nLoaded dependency workflow: " << depWorkflow.id
                  << " with " << depWorkflow.rules.size() << " rules\n";

        // ================================================================
        // 4. Serialize workflow back to JSON
        // ================================================================
        std::string prettyJson = JsonLoader::saveWorkflowPretty(workflow);
        std::cout << "\nSerialized workflow (pretty):\n" << prettyJson << "\n";

        // ================================================================
        // 5. JSON repository persistence
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

        std::cout << "\nJSON extension example complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
