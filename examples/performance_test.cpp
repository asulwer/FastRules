// performance_test.cpp
// Measure workflow execution performance with engine pool

#include <fastrules.hpp>
#include <iostream>
#include <chrono>
#include <vector>

struct Customer {
    int age;
    std::string name;
    bool isActive;
};

int main() {
    try {
        // Create engine and register type
        fastrules::LuaEngine engine;
        engine.registerType<Customer>("Customer", [](auto& reg) {
            reg.bind("age", &Customer::age);
            reg.bind("name", &Customer::name);
            reg.bind("isActive", &Customer::isActive);
        });

        // Create workflow with multiple rules
        fastrules::Workflow workflow;
        workflow.id = 1;
        workflow.description = "Performance test";

        // Add 50 rules
        for (int i = 1; i <= 50; ++i) {
            auto rule = std::make_shared<fastrules::Rule>();
            rule->id = i;
            rule->name = "rule" + std::to_string(i);
            rule->expression = "customer.age >= " + std::to_string(i);
            rule->isActive = true;
            workflow.rules.push_back(rule);
        }

        // Compile
        auto compileStart = std::chrono::high_resolution_clock::now();
        workflow.compile(engine);
        auto compileEnd = std::chrono::high_resolution_clock::now();
        auto compileTime = std::chrono::duration_cast<std::chrono::microseconds>(compileEnd - compileStart).count();

        std::cout << "Compiled " << workflow.rules.size() << " rules in " << compileTime << " us\n";
        std::cout << "Engine pool size: " << workflow.rules.size() << " clones\n\n";

        // Test sequential execution
        Customer customer{100, "Test", true};
        std::vector<fastrules::RuleParameter> params;
        params.emplace_back("customer", &customer);

        const int iterations = 100;
        
        // Sequential execution timing
        auto seqStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto results = workflow.execute(engine, params);
        }
        auto seqEnd = std::chrono::high_resolution_clock::now();
        auto seqTime = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart).count();

        std::cout << "Sequential execution:\n";
        std::cout << "  " << iterations << " iterations in " << seqTime << " us\n";
        std::cout << "  Average: " << (seqTime / iterations) << " us/iteration\n";
        std::cout << "  Throughput: " << (1000000.0 * iterations / seqTime) << " executions/sec\n\n";

        // Parallel execution timing
        auto parStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto results = workflow.executeParallel(engine, params);
        }
        auto parEnd = std::chrono::high_resolution_clock::now();
        auto parTime = std::chrono::duration_cast<std::chrono::microseconds>(parEnd - parStart).count();

        std::cout << "Parallel execution:\n";
        std::cout << "  " << iterations << " iterations in " << parTime << " us\n";
        std::cout << "  Average: " << (parTime / iterations) << " us/iteration\n";
        std::cout << "  Throughput: " << (1000000.0 * iterations / parTime) << " executions/sec\n";
        std::cout << "  Speedup: " << (seqTime / (double)parTime) << "x\n\n";

        // Test with dependencies
        fastrules::Workflow workflowWithDeps;
        workflowWithDeps.id = 2;
        
        for (int i = 1; i <= 20; ++i) {
            auto rule = std::make_shared<fastrules::Rule>();
            rule->id = i;
            rule->name = "rule" + std::to_string(i);
            if (i > 1) {
                rule->dependsOnRuleName = "rule" + std::to_string(i-1);
            }
            rule->expression = "customer.age >= " + std::to_string(i);
            workflowWithDeps.rules.push_back(rule);
        }

        workflowWithDeps.compile(engine);

        auto depStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto results = workflowWithDeps.execute(engine, params);
        }
        auto depEnd = std::chrono::high_resolution_clock::now();
        auto depTime = std::chrono::duration_cast<std::chrono::microseconds>(depEnd - depStart).count();

        std::cout << "Execution with dependencies:\n";
        std::cout << "  " << iterations << " iterations in " << depTime << " us\n";
        std::cout << "  Average: " << (depTime / iterations) << " us/iteration\n";
        std::cout << "  Throughput: " << (1000000.0 * iterations / depTime) << " executions/sec\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
