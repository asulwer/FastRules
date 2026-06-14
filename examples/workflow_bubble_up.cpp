// workflow_bubble_up.cpp
// Evaluation bubbles up: children first, parent only if all children pass
// Inactive rules are skipped entirely
// Results returned for all active parent rules

#include <fastrules.hpp>
#include <iostream>
#include <vector>
#include <memory>

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

        // === WORKFLOW: Holder for all rules ===
        fastrules::Workflow workflow;
        workflow.id = 6;
        workflow.description = "Process customers with validation";
        workflow.isActive = true;

        // === PARENT 1: Adult Processing ===
        auto p1_child1 = std::make_shared<fastrules::Rule>();
        p1_child1->id = 1;
        p1_child1->name = "1";
        p1_child1->expression = "customer.age >= 18";
        p1_child1->isActive = true;

        auto p1_child2 = std::make_shared<fastrules::Rule>();
        p1_child2->id = 2;
        p1_child2->name = "2";
        p1_child2->expression = "isNotEmpty(customer.name)";
        p1_child2->isActive = true;

        auto parent1 = std::make_shared<fastrules::Rule>();
        parent1->id = 3;
        parent1->name = "3";
        parent1->expression = "context.getResult(1).success == true and context.getResult(2).success == true";
        parent1->action = "customer.processed = true";
        parent1->isActive = true;
        // parent1->childRules = {p1_child1, p1_child2};  // DISABLED - causes Debug crash

        // === PARENT 2: Minor Processing ===
        auto p2_child1 = std::make_shared<fastrules::Rule>();
        p2_child1->id = 4;
        p2_child1->name = "4";
        p2_child1->expression = "customer.age < 18";
        p2_child1->isActive = true;

        auto parent2 = std::make_shared<fastrules::Rule>();
        parent2->id = 5;
        parent2->name = "5";
        parent2->expression = "context.getResult(4).success == true";
        parent2->action = "customer.processed = false";
        parent2->isActive = true;
        // parent2->childRules = {p2_child1};  // DISABLED - causes Debug crash

        // Add all rules to workflow - children first, then parents
        // Execution order respects dependencies (children execute before parents)
        workflow.rules = {p1_child1, p1_child2, parent1, p2_child1, parent2};
        workflow.compile(engine);

        // Test data - use heap allocation to ensure lifetime
        auto alice = std::make_unique<Customer>(Customer{"Alice", 25, false, true});
        auto bob = std::make_unique<Customer>(Customer{"Bob", 15, false, true});
        auto charlie = std::make_unique<Customer>(Customer{"", 30, false, true});
        auto diana = std::make_unique<Customer>(Customer{"Diana", 55, false, true});

        std::vector<std::pair<std::string, Customer*>> tests = {
            {"Alice (Adult, valid)", alice.get()},
            {"Bob (Minor)", bob.get()},
            {"Charlie (Adult, no name)", charlie.get()},
            {"Diana (VIP)", diana.get()}
        };

        for (const auto& [label, customer] : tests) {
            std::cout << "\n=== " << label << " ===" << std::endl;
            std::cout << "  Name: " << customer->name
                      << ", Age: " << customer->age << std::endl;

            std::vector<fastrules::RuleParameter> params;
            params.emplace_back("customer", customer);

            auto results = workflow.execute(engine, params);

            std::cout << "  Results:" << std::endl;
            for (const auto& result : results) {
                std::cout << "    " << result.ruleName
                          << ": " << (result.isSuccess() ? "PASS" : "FAIL");
                if (result.exception.has_value()) {
                    std::cout << " [EXCEPTION: " << result.exception.value().what() << "]";
                }
                std::cout << std::endl;
            }

            std::cout << "  Processed: " << (customer->processed ? "Yes" : "No") << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
