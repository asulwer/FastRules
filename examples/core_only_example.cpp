// core_only_example.cpp
// Comprehensive example using ONLY fastrules core — no extensions.
// Demonstrates: rules, workflows, Lua expressions, actions, dependencies,
// parallel execution, streaming, timeouts, type registration.

#include <fastrules.hpp>
#include <iostream>
#include <vector>
#include <cmath>

using namespace fastrules;

int main() {
    try {
        std::cout << "=== FastRules Core-Only Example ===" << std::endl;
        std::cout << "No JSON, XML, or DB extensions used." << std::endl << std::endl;

        // ================================================================
        // 1. Setup Lua engine
        // ================================================================
        LuaEngine engine;

        // ================================================================
        // 2. Register a C++ struct type (available in Lua expressions)
        // ================================================================
        struct Customer {
            int id;
            std::string name;
            int age;
            double creditScore;
        };

        engine.registerType<Customer>("Customer", [](auto& ut) {
            ut["id"] = &Customer::id;
            ut["name"] = &Customer::name;
            ut["age"] = &Customer::age;
            ut["creditScore"] = &Customer::creditScore;
        });

        // ================================================================
        // 3. Create rules programmatically (no JSON loading)
        // ================================================================
        auto adultCheck = std::make_shared<Rule>();
        adultCheck->id = "adult-check";
        adultCheck->expression = "customer.age >= 18";
        adultCheck->action = "isAdult = true";
        adultCheck->timeout = std::chrono::milliseconds(100);

        auto creditCheck = std::make_shared<Rule>();
        creditCheck->id = "credit-check";
        creditCheck->expression = "customer.creditScore >= 650";
        creditCheck->action = "isCreditWorthy = true";
        creditCheck->dependsOnRuleId = "adult-check";  // Runs after adult-check
        creditCheck->timeout = std::chrono::milliseconds(200);

        auto nameCheck = std::make_shared<Rule>();
        nameCheck->id = "name-check";
        nameCheck->expression = "isNotEmpty(customer.name)";
        nameCheck->action = "hasValidName = true";

        // ================================================================
        // 4. Build workflow
        // ================================================================
        Workflow workflow;
        workflow.id = "customer-validation";
        workflow.description = "Validate adult customers with good credit";
        workflow.rules.push_back(adultCheck);
        workflow.rules.push_back(creditCheck);
        workflow.rules.push_back(nameCheck);

        // ================================================================
        // 5. Compile workflow (one-time setup)
        // ================================================================
        workflow.compile(engine);
        std::cout << "Workflow compiled: " << workflow.rules.size() << " rules" << std::endl;

        // ================================================================
        // 6. Execute with valid customer
        // ================================================================
        std::cout << "\n--- Valid Customer (Adult, Good Credit) ---" << std::endl;
        Customer customer1{1, "Alice", 30, 720};
        std::vector<RuleParameter> params;
        params.emplace_back("customer", "Customer", std::any(&customer1));

        auto results = workflow.execute(engine, params);
        for (const auto& r : results) {
            std::cout << r.ruleId << ": " << (r.isSuccess() ? "PASS" : "FAIL");
            if (r.isSuccess()) {
                std::cout << " (time: " << r.metrics.totalExecutionTime.count() << "ns)";
            }
            std::cout << std::endl;
        }

        // ================================================================
        // 7. Execute with invalid customer (minor)
        // ================================================================
        std::cout << "\n--- Invalid Customer (Minor) ---" << std::endl;
        Customer customer2{2, "Bob", 16, 800};
        params.clear();
        params.emplace_back("customer", "Customer", std::any(&customer2));

        results = workflow.execute(engine, params);
        for (const auto& r : results) {
            std::cout << r.ruleId << ": " << (r.isSuccess() ? "PASS" : "FAIL");
            if (r.skipped) {
                std::cout << " (SKIPPED — dependency failed)";
            }
            std::cout << std::endl;
        }

        // ================================================================
        // 8. Parallel execution
        // ================================================================
        std::cout << "\n--- Parallel Execution ---" << std::endl;
        Customer customer3{3, "Charlie", 25, 680};
        params.clear();
        params.emplace_back("customer", "Customer", std::any(&customer3));

        auto parResults = workflow.executeParallel(engine, params);
        for (const auto& r : parResults) {
            std::cout << r.ruleId << ": " << (r.isSuccess() ? "PASS" : "FAIL") << std::endl;
        }

        // ================================================================
        // 9. Streaming results (one at a time)
        // ================================================================
        std::cout << "\n--- Streaming Results ---" << std::endl;
        Customer customer4{4, "Diana", 40, 750};
        params.clear();
        params.emplace_back("customer", "Customer", std::any(&customer4));

        auto stream = workflow.executeStreaming(engine, params);
        for (const auto& result : stream) {
            std::cout << result.ruleId << ": " << (result.isSuccess() ? "PASS" : "FAIL") << std::endl;
        }

        // ================================================================
        // 10. Rule with priority and timeout
        // ================================================================
        std::cout << "\n--- Priority & Timeout ---" << std::endl;
        auto slowRule = std::make_shared<Rule>();
        slowRule->id = "slow-rule";
        slowRule->expression = "true";
        slowRule->action = "x = 1";
        slowRule->priority = 999;  // Runs first
        slowRule->timeout = std::chrono::milliseconds(50);

        Workflow priorityWorkflow;
        priorityWorkflow.id = "priority-test";
        priorityWorkflow.rules.push_back(slowRule);
        priorityWorkflow.rules.push_back(adultCheck);  // priority 0, runs after
        priorityWorkflow.compile(engine);

        Customer customer5{5, "Eve", 35, 600};
        params.clear();
        params.emplace_back("customer", "Customer", std::any(&customer5));

        auto priorityResults = priorityWorkflow.execute(engine, params);
        for (const auto& r : priorityResults) {
            std::cout << r.ruleId << " (priority " << 
                (r.ruleId == "slow-rule" ? "999" : "0") << "): " <<
                (r.isSuccess() ? "PASS" : "FAIL") << std::endl;
        }

        // ================================================================
        // 11. Access Lua variables set by actions
        // ================================================================
        std::cout << "\n--- Action Results ---" << std::endl;
        Customer customer6{6, "Frank", 28, 700};
        params.clear();
        params.emplace_back("customer", "Customer", std::any(&customer6));

        (void)workflow.execute(engine, params);

        // Read back variables set by actions from Lua state
        sol::state& lua = engine.state();
        bool isAdult = lua["isAdult"].get_or(false);
        bool isCreditWorthy = lua["isCreditWorthy"].get_or(false);
        bool hasValidName = lua["hasValidName"].get_or(false);

        std::cout << "isAdult = " << (isAdult ? "true" : "false") << std::endl;
        std::cout << "isCreditWorthy = " << (isCreditWorthy ? "true" : "false") << std::endl;
        std::cout << "hasValidName = " << (hasValidName ? "true" : "false") << std::endl;

        // ================================================================
        // 12. Reset Lua state (important for long-running apps)
        // ================================================================
        (void)engine.resetState();
        std::cout << "\nEngine state reset." << std::endl;

        std::cout << "\n=== Core-Only Example Complete ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}


