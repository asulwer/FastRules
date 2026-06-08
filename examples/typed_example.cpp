// typed_example.cpp
// Demonstrates strongly-typed parameters
// Actions mutate C++ objects directly via Lua expressions

#include <fastrules.hpp>
#include <iostream>
#include <vector>

// Define a C++ struct with comparison operators for sol2 automagic
struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;
    bool isActive = true;

    // Comparison operators required by sol2 automagic
    bool operator==(const Customer& other) const {
        return name == other.name && age == other.age &&
               processed == other.processed && isActive == other.isActive;
    }

    bool operator<=(const Customer& other) const {
        return age <= other.age;
    }

    bool operator<(const Customer& other) const {
        return age < other.age;
    }
};

int main() {
    try {
        // Create a Lua engine
        fastrules::LuaEngine engine;

        // Register the Customer type with Lua
        // This makes customer.name, customer.age, etc. accessible in expressions
        engine.registerType<Customer>("Customer", [](auto& ut) {
            ut["name"] = &Customer::name;
            ut["age"] = &Customer::age;
            ut["processed"] = &Customer::processed;
            ut["isActive"] = &Customer::isActive;
        });

        // Create a workflow with rules using typed expressions
        fastrules::Workflow workflow;
        workflow.description = "Customer validation with typed parameters";

        // Rule 1: Check if customer is an adult
        // Action directly modifies the C++ object: customer.processed = true
        auto adultCheck = std::make_shared<fastrules::Rule>();
        adultCheck->id = "adult-check";
        adultCheck->description = "Adult customer check";
        adultCheck->expression = "customer.age >= 18";
        adultCheck->action = "customer.processed = true";  // Direct mutation!
        workflow.rules.push_back(adultCheck);

        // Rule 2: Check name is not empty
        auto nameCheck = std::make_shared<fastrules::Rule>();
        nameCheck->id = "name-check";
        nameCheck->description = "Name not empty check";
        nameCheck->expression = "isNotEmpty(customer.name)";
        workflow.rules.push_back(nameCheck);

        // Rule 3: Check customer is active
        auto activeCheck = std::make_shared<fastrules::Rule>();
        activeCheck->id = "active-check";
        activeCheck->description = "Customer is active";
        activeCheck->expression = "customer.isActive == true";
        workflow.rules.push_back(activeCheck);

        // Compile the workflow
        workflow.compile(engine);

        // Test data
        Customer adult{"Alice", 25, false, true};
        Customer minor{"Bob", 15, false, true};
        Customer invalid{"", 30, false, true};

        std::vector<std::pair<std::string, Customer*>> tests = {
            {"Adult", &adult},
            {"Minor", &minor},
            {"Invalid Name", &invalid}
        };

        for (const auto& [label, customer] : tests) {
            std::cout << "\n=== " << label << " ===" << std::endl;

            // Pass the Customer pointer as a typed parameter
            std::vector<fastrules::RuleParameter> params;
            params.emplace_back("customer", customer);

            auto results = workflow.execute(engine, params);

            for (const auto& result : results) {
                std::cout << "  Rule: " << result.ruleId
                          << " - Success: " << (result.isSuccess() ? "Yes" : "No");
                if (result.exception.has_value()) {
                    std::cout << " [Exception: " << result.exception.value().what() << "]";
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


