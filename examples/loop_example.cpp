// loop_example.cpp
// Register types once, execute in a loop with different parameters

#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>
#include "path_utils.hpp"
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;
    bool isActive = true;

    bool operator==(const Customer& o) const { return name == o.name && age == o.age; }
    bool operator<=(const Customer& o) const { return age <= o.age; }
    bool operator<(const Customer& o) const { return age < o.age; }
};

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    try {
        // 1. CREATE ENGINE - one time
        fastrules::LuaEngine engine;

        // 2. REGISTER TYPES - one time
        // This binds Customer properties to Lua. Do this once at startup.
        engine.registerType<Customer>("Customer", [](auto& reg) {
            reg.bind("name", &Customer::name);
            reg.bind("age", &Customer::age);
            reg.bind("processed", &Customer::processed);
            reg.bind("isActive", &Customer::isActive);
        });

        // 3. CREATE WORKFLOW - one time
        std::string jsonPath = fastrules_examples::resolveDataPath("data/json/customer_rules.json");
        if (argc > 1) jsonPath = argv[1];
        auto jsonStr = readFile(jsonPath);
        auto workflow = fastrules::JsonLoader::loadWorkflow(jsonStr);
        workflow.compile(engine);  // Also one time

        // 4. LOOP - different customers, same engine/workflow
        std::vector<Customer> customers = {
            {"Alice", 25, false, true},
            {"Bob", 15, false, true},
            {"Charlie", 30, false, false},
            {"Diana", 20, false, true},
            {"", 40, false, true}
        };

        std::cout << "Processing " << customers.size() << " customers..." << std::endl;

        for (size_t i = 0; i < customers.size(); ++i) {
            auto& customer = customers[i];

            // 5. CREATE PARAMETERS - per execution
            // Different customer each time, same parameter name
            std::vector<fastrules::RuleParameter> params;
            params.emplace_back("customer", &customer);

            // 6. EXECUTE - evaluate rules for this customer
            auto results = workflow.execute(engine, params);

            // Print results
            std::cout << "\n[" << i + 1 << "] " << customer.name
                      << " (age " << customer.age << ")" << std::endl;

            for (const auto& result : results) {
                std::cout << "    " << result.ruleId
                          << ": " << (result.isSuccess() ? "PASS" : "FAIL")
                          << std::endl;
            }

            std::cout << "    processed: " << (customer.processed ? "Yes" : "No") << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
