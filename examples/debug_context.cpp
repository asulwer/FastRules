#include <fastrules.hpp>
#include <iostream>

struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;

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
        });

        // Child rule
        auto child = std::make_shared<fastrules::Rule>();
        child->id = 1;
        child->expression = "customer.age >= 18";
        child->isActive = true;

        // Child rule 2
        auto child2 = std::make_shared<fastrules::Rule>();
        child2->id = 2;
        child2->expression = "isNotEmpty(customer.name)";
        child2->isActive = true;

        // Parent rule
        auto parent = std::make_shared<fastrules::Rule>();
        parent->id = 3;
        parent->expression = "context.getResult(6).success == true and context.getResult(7).success == true";
        parent->action = "customer.processed = true";
        parent->isActive = true;
        parent->childRules = {child, child2};

        // Parent 2 (minor)
        auto minorChild = std::make_shared<fastrules::Rule>();
        minorChild->id = 4;
        minorChild->expression = "customer.age < 18";
        minorChild->isActive = true;

        auto parent2 = std::make_shared<fastrules::Rule>();
        parent2->id = 5;
        parent2->expression = "context.getResult(8).success == true";
        parent2->action = "customer.processed = false";
        parent2->isActive = true;
        parent2->childRules = {minorChild};

        fastrules::Workflow workflow;
        workflow.rules = {parent, parent2};
        workflow.compile(engine);

        Customer customer{"Alice", 25, false};
        std::vector<fastrules::RuleParameter> params;
        params.emplace_back("customer", &customer);

        auto results = workflow.execute(engine, params);

        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleId
                      << " - Success: " << (result.isSuccess() ? "Yes" : "No");
            if (result.exception.has_value()) {
                std::cout << " [Exception: " << result.exception.value().what() << "]";
            }
            std::cout << std::endl;
        }

        std::cout << "Processed: " << (customer.processed ? "Yes" : "No") << std::endl;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}


