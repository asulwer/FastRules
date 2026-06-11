// loop_example.cpp
// Register types once, execute in a loop with different parameters
// Creates workflow and rules programmatically in C++ (no JSON)

#include <fastrules.hpp>
#include <iostream>
#include <vector>

struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;
    bool isActive = true;

    bool operator==(const Customer& o) const { return name == o.name && age == o.age; }
    bool operator<=(const Customer& o) const { return age <= o.age; }
    bool operator<(const Customer& o) const { return age < o.age; }
};

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;  // Unused
    
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

        // 3. CREATE WORKFLOW AND RULES PROGRAMMATICALLY - one time
        fastrules::Workflow workflow;
        workflow.id = 1;
        workflow.name = "Customer Processing";
        
        // Rule 1: Validate Age
        fastrules::Rule rule1;
        rule1.id = 1;
        rule1.name = "Validate Age";
        rule1.expression = "customer.age >= 18";
        // Action: set customer.processed = true when rule passes
        fastrules::Action action1;
        action1.type = "set_field";
        action1.target = "customer.processed";
        action1.value = true;
        rule1.actions.push_back(action1);
        workflow.rules.push_back(rule1);
        
        // Rule 2: Check Active
        fastrules::Rule rule2;
        rule2.id = 2;
        rule2.name = "Check Active";
        rule2.expression = "customer.isActive";
        // No actions for this rule
        workflow.rules.push_back(rule2);
        
        // Compile the workflow
        workflow.compile(engine);

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
