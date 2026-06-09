// simple_example.cpp
// Basic usage of fastrules with inline rules

#include <fastrules.hpp>
#include <iostream>
#include <vector>

int main() {
    try {
        // Create a Lua engine
        fastrules::LuaEngine engine;

        // Create a workflow with rules
        fastrules::Workflow workflow;
        workflow.description = "Customer validation";

        // Rule 1: Check if customer is an adult (age >= 18)
        auto adultCheck = std::make_shared<fastrules::Rule>();
        adultCheck->id = 1;
        adultCheck->description = "Adult customer check";
        adultCheck->expression = "age >= 18";
        workflow.rules.push_back(adultCheck);

        // Rule 2: Check name is not empty
        auto nameCheck = std::make_shared<fastrules::Rule>();
        nameCheck->id = 2;
        nameCheck->description = "Name not empty check";
        nameCheck->expression = "isNotEmpty(name)";
        workflow.rules.push_back(nameCheck);

        // Compile the workflow
        workflow.compile(engine);

        // Execute for adult customer
        std::cout << "=== Adult Customer ===" << std::endl;
        std::vector<fastrules::RuleParameter> params;
        params.emplace_back("age", 25);
        params.emplace_back("name", std::string("Alice"));
        auto results = workflow.execute(engine, params);
        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleId
                      << " - Success: " << (result.isSuccess() ? "Yes" : "No")
                      << std::endl;
        }

        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleId
                      << " - Success: " << (result.isSuccess() ? "Yes" : "No")
                      << std::endl;
        }

        // Execute for minor customer
        std::cout << "\n=== Minor Customer ===" << std::endl;
        params.clear();
        params.emplace_back("age", 15);
        params.emplace_back("name", std::string("Bob"));

        results = workflow.execute(engine, params);

        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleId
                      << " - Success: " << (result.isSuccess() ? "Yes" : "No")
                      << std::endl;
        }

        // Demonstrate parallel execution
        std::cout << "\n=== Parallel Execution ===" << std::endl;
        params.clear();
        params.emplace_back("age", 30);
        params.emplace_back("name", std::string("Charlie"));

        auto parResults = workflow.executeParallel(engine, params);
        for (const auto& result : parResults) {
            std::cout << "Rule " << result.ruleId
                      << " - Success: " << (result.isSuccess() ? "Yes" : "No")
                      << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}


