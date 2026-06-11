// example_hierarchical_rules.cpp
// Loads and executes workflow from hierarchical_rules.json

#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace fastrules;

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open: " + path);
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

int main() {
    try {
        LuaEngine engine;

        std::string jsonPath = (std::filesystem::current_path() / "hierarchical_rules.json").string();
        std::string workflowJson = readFile(jsonPath);

        auto workflow = JsonLoader::loadWorkflow(workflowJson);
        std::cout << "Loaded hierarchical rules workflow: " << workflow.id << "\n";
        std::cout << "  Rules: " << workflow.rules.size() << "\n";

        // Execute with sample customer
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

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
