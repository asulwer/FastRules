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

        engine.registerType<Customer>("Customer", [](auto& reg) {
            reg.bind("name", &Customer::name);
            reg.bind("age", &Customer::age);
            reg.bind("processed", &Customer::processed);
            reg.bind("isActive", &Customer::isActive);
        });

        // Child rule 1: Age check
        auto child1 = std::make_shared<fastrules::Rule>();
        child1->id = 1;
        child1->name = "1";
        child1->expression = "customer.age >= 18";
        child1->isActive = true;
        child1->priority = 0;

        // Child rule 2: Active check
        auto child2 = std::make_shared<fastrules::Rule>();
        child2->id = 2;
        child2->name = "2";
        child2->expression = "customer.isActive == true";
        child2->isActive = true;
        child2->priority = 1;

        // Parent rule: only executes if both children pass
        // Uses context.getResult() to check child results
        auto parent = std::make_shared<fastrules::Rule>();
        parent->id = 3;
        parent->name = "3";
        parent->expression = "context.getResult(1).success == true and context.getResult(2).success == true";
        parent->action = "customer.processed = true";
        parent->isActive = true;
        parent->priority = 2;

        // Add children to parent's childRules
        // Parent is top-level, children are nested
        // parent->childRules = {child1, child2};  // DISABLED - causes Debug crash

        fastrules::Workflow workflow;
        // Include child rules in workflow so they execute before parent
        workflow.rules = {child1, child2, parent};
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
                std::cout << "  Rule: " << result.ruleName
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
