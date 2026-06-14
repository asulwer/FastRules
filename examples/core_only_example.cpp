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

        engine.registerType<Customer>("Customer", [](auto& reg) {
            reg.bind("id", &Customer::id);
            reg.bind("name", &Customer::name);
            reg.bind("age", &Customer::age);
            reg.bind("creditScore", &Customer::creditScore);
        });

        // ================================================================
        // 3. Create rules programmatically (no JSON loading)
        // ================================================================
        auto adultCheck = std::make_shared<Rule>();
        adultCheck->id = 1;
        adultCheck->name = "adultCheck";
        adultCheck->expression = "customer.age >= 18";
        adultCheck->action = "isAdult = true";
        adultCheck->timeout = std::chrono::milliseconds(100);

        auto creditCheck = std::make_shared<Rule>();
        creditCheck->id = 2;
        creditCheck->name = "creditCheck";
        creditCheck->expression = "customer.creditScore >= 650";
        creditCheck->action = "isCreditWorthy = true";
        creditCheck->dependsOnRuleName = "adultCheck";  // Runs after adult-check
        creditCheck->timeout = std::chrono::milliseconds(200);

        auto nameCheck = std::make_shared<Rule>();
        nameCheck->id = 3;
        nameCheck->name = "nameCheck";
        nameCheck->expression = "isNotEmpty(customer.name)";
        nameCheck->action = "hasValidName = true";

        // ================================================================
        // 4. Build workflow
        // ================================================================
        Workflow workflow;
        workflow.id = 5;
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
        params.emplace_back("customer", &customer1);

        auto results = workflow.execute(engine, params);
        for (const auto& r : results) {
            std::cout << r.ruleName << ": " << (r.isSuccess() ? "PASS" : "FAIL");
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
        params.emplace_back("customer", &customer2);

        results = workflow.execute(engine, params);
        for (const auto& r : results) {
            std::cout << r.ruleName << ": " << (r.isSuccess() ? "PASS" : "FAIL");
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
        params.emplace_back("customer", &customer3);

        auto parResults = workflow.executeParallel(engine, params);
        for (const auto& r : parResults) {
            std::cout << r.ruleName << ": " << (r.isSuccess() ? "PASS" : "FAIL") << std::endl;
        }

        // ================================================================
        // 9. Streaming results (one at a time)
        // ================================================================
        std::cout << "\n--- Streaming Results ---" << std::endl;
        Customer customer4{4, "Diana", 40, 750};
        params.clear();
        params.emplace_back("customer", &customer4);

        auto stream = workflow.executeStreaming(engine, params);
        for (const auto& result : stream) {
            std::cout << result.ruleName << ": " << (result.isSuccess() ? "PASS" : "FAIL") << std::endl;
        }

        // ================================================================
        // 10. Rule with priority and timeout
        // ================================================================
        std::cout << "\n--- Priority & Timeout ---" << std::endl;
        auto slowRule = std::make_shared<Rule>();
        slowRule->id = 4;
        slowRule->name = "slowRule";
        slowRule->expression = "true";
        slowRule->action = "x = 1";
        slowRule->priority = 999;  // Runs first
        slowRule->timeout = std::chrono::milliseconds(50);

        Workflow priorityWorkflow;
        priorityWorkflow.id = 6;
        priorityWorkflow.rules.push_back(slowRule);
        auto adultCheck2 = std::make_shared<Rule>();
        adultCheck2->id = 5;
        adultCheck2->name = "adultCheck";
        adultCheck2->expression = "customer.age >= 18";
        priorityWorkflow.rules.push_back(adultCheck2);  // priority 0, runs after
        priorityWorkflow.compile(engine);

        Customer customer5{5, "Eve", 35, 600};
        params.clear();
        params.emplace_back("customer", &customer5);

        auto priorityResults = priorityWorkflow.execute(engine, params);
        for (const auto& r : priorityResults) {
            std::cout << r.ruleName << " (priority " << 
                (r.ruleName == "slowRule" ? "999" : "0") << "): " <<
                (r.isSuccess() ? "PASS" : "FAIL") << std::endl;
        }

        // ================================================================
        // 11. Access Lua variables set by actions
        // NOTE: engine.getGlobal() is not available in this API.
        // Use RuleResult and actions that mutate C++ objects instead.
        // ================================================================
        std::cout << "\n--- Action Results ---" << std::endl;
        Customer customer6{6, "Frank", 28, 700};
        params.clear();
        params.emplace_back("customer", &customer6);

        (void)workflow.execute(engine, params);

        std::cout << "Actions executed (variables set in Lua state)." << std::endl;
        std::cout << "Note: Use C++ object mutation via actions for observable side effects." << std::endl;

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
