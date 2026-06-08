// parent_child_example.cpp
// Parent rule with two children - action only executes if both children pass

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

int main() {
    try {
        fastrules::LuaEngine engine;

        engine.registerType<Customer>("Customer", [](auto& ut) {
            ut["name"] = &Customer::name;
            ut["age"] = &Customer::age;
            ut["processed"] = &Customer::processed;
            ut["isActive"] = &Customer::isActive;
        });

        // Child rule 1: Age check
        auto child1 = std::make_shared<fastrules::Rule>();
        child1->id = "age-check";
        child1->expression = "customer.age >= 18";
        child1->isActive = true;
        child1->priority = 0;

        // Child rule 2: Active check
        auto child2 = std::make_shared<fastrules::Rule>();
        child2->id = "active-check";
        child2->expression = "customer.isActive == true";
        child2->isActive = true;
        child2->priority = 1;

        // Parent rule: only executes if both children pass
        // Uses context.getResult() to check child results
        auto parent = std::make_shared<fastrules::Rule>();
        parent->id = "process-customer";
        parent->expression = "context.getResult('age-check').success == true and context.getResult('active-check').success == true";
        parent->action = "customer.processed = true";
        parent->isActive = true;
        parent->priority = 2;

        // Add children to parent's childRules
        // Parent depends on both children
        parent->childRules = {child1, child2};

        fastrules::Workflow workflow;
        workflow.rules = {parent};  // Parent is top-level, children are nested
        workflow.compile(engine);

        // Test data
        Customer adultActive{"Alice", 25, false, true};
        Customer minorActive{"Bob", 15, false, true};
        Customer adultInactive{"Charlie", 30, false, false};
        Customer minorInactive{"Diana", 16, false, false};

        std::vector<std::pair<std::string, Customer*>> tests = {
            {"Adult + Active", &adultActive},
            {"Minor + Active", &minorActive},
            {"Adult + Inactive", &adultInactive},
            {"Minor + Inactive", &minorInactive}
        };

        for (const auto& [label, customer] : tests) {
            std::cout << "\n=== " << label << " ===" << std::endl;
            std::cout << "  Name: " << customer->name
                      << ", Age: " << customer->age
                      << ", Active: " << (customer->isActive ? "Yes" : "No") << std::endl;

            std::vector<fastrules::RuleParameter> params;
            params.emplace_back("customer", customer);

            auto results = workflow.execute(engine, params);

            for (const auto& result : results) {
                std::cout << "  Rule: " << result.ruleId
                          << " - Success: " << (result.isSuccess() ? "Yes" : "No")
                          << std::endl;
            }

            std::cout << "  Processed: " << (customer->processed ? "Yes" : "No") << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}


