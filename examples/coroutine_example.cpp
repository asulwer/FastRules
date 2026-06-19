/**
 * @file coroutine_example.cpp
 * @brief Example demonstrating C++20 coroutines with FastRules
 * 
 * This example shows how to use C++20 coroutines for async rule execution,
 * providing a more natural and efficient way to handle asynchronous operations.
 */

#include "fastrules/fastrules.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <coroutine>

using namespace fastrules;

// Simple coroutine task that returns a value after some delay
struct DelayedTask {
    struct promise_type {
        int value_;
        
        auto get_return_object() {
            return DelayedTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(int value) {
            value_ = value;
        }
        
        void unhandled_exception() {
            std::terminate();
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    handle_type handle_;
    
    DelayedTask(handle_type h) : handle_(h) {}
    
    ~DelayedTask() {
        if (handle_) handle_.destroy();
    }
    
    DelayedTask(const DelayedTask&) = delete;
    DelayedTask& operator=(const DelayedTask&) = delete;
    
    DelayedTask(DelayedTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    DelayedTask& operator=(DelayedTask&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Check if coroutine is done
    bool done() const {
        return handle_.done();
    }
    
    // Resume coroutine
    void resume() {
        if (!handle_.done()) {
            handle_.resume();
        }
    }
    
    // Get result
    int get() {
        while (!handle_.done()) {
            handle_.resume();
        }
        return handle_.promise().value_;
    }
};

// Coroutine that delays for specified milliseconds and returns a value
DelayedTask delayed_value(int value, int delay_ms) {
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    co_return value;
}

// Coroutine that executes a rule and returns the result
struct RuleExecutionTask {
    struct promise_type {
        RuleResult result_;
        std::exception_ptr exception_;
        
        auto get_return_object() {
            return RuleExecutionTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(RuleResult result) {
            result_ = std::move(result);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    handle_type handle_;
    
    RuleExecutionTask(handle_type h) : handle_(h) {}
    
    ~RuleExecutionTask() {
        if (handle_) handle_.destroy();
    }
    
    RuleExecutionTask(const RuleExecutionTask&) = delete;
    RuleExecutionTask& operator=(const RuleExecutionTask&) = delete;
    
    RuleExecutionTask(RuleExecutionTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    RuleExecutionTask& operator=(RuleExecutionTask&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Check if coroutine is done
    bool done() const {
        return handle_.done();
    }
    
    // Resume coroutine
    void resume() {
        if (!handle_.done()) {
            handle_.resume();
        }
    }
    
    // Get result
    RuleResult get() {
        while (!handle_.done()) {
            handle_.resume();
        }
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return handle_.promise().result_;
    }
};

// Coroutine that executes a rule asynchronously
RuleExecutionTask async_execute_rule(std::shared_ptr<Rule> rule, 
                                    LuaEngine& engine,
                                    const std::vector<RuleParameter>& parameters) {
    RuleContext context;
    RuleResult result;
    
    try {
        result = rule->execute(engine, context, parameters);
    } catch (...) {
        co_return RuleResult{}; // Exception will be stored in promise
    }
    
    co_return result;
}

int main() {
    std::cout << "FastRules Coroutine Example\n";
    std::cout << "==========================\n\n";
    
    try {
        // Example 1: Simple delayed value coroutine
        std::cout << "1. Simple delayed value coroutine:\n";
        auto task1 = delayed_value(42, 100);
        std::cout << "   Result: " << task1.get() << "\n\n";
        
        // Example 2: Rule execution with coroutines
        std::cout << "2. Rule execution with coroutines:\n";
        
        // Create a Lua engine
        LuaEngine engine;
        
        // Create a simple rule
        auto rule = std::make_shared<Rule>();
        rule->id = 1;
        rule->name = "simple-coroutine-rule";
        rule->expression = "value > 10";
        rule->action = "result.success = true; result.value = value * 2";
        
        // Compile the rule
        rule->compile(engine);
        
        // Create parameters
        std::vector<RuleParameter> parameters = {
            {"value", 15}  // This should make the expression true
        };
        
        // Execute rule with coroutine
        auto rule_task = async_execute_rule(rule, engine, parameters);
        auto rule_result = rule_task.get();
        
        std::cout << "   Rule name: " << rule_result.ruleName << "\n";
        std::cout << "   Success: " << (rule_result.success ? "true" : "false") << "\n";
        std::cout << "\n";
        
        // Example 3: Multiple concurrent coroutines
        std::cout << "3. Multiple concurrent coroutines:\n";
        
        // Create multiple delayed tasks
        std::vector<DelayedTask> tasks;
        for (int i = 1; i <= 5; ++i) {
            tasks.push_back(delayed_value(i * 10, i * 50));
        }
        
        // Collect results
        std::cout << "   Results: ";
        for (auto& task : tasks) {
            std::cout << task.get() << " ";
        }
        std::cout << "\n\n";
        
        // Example 4: Coroutine-based workflow execution
        std::cout << "4. Coroutine-based workflow execution:\n";
        
        // Create a workflow with multiple rules
        Workflow workflow;
        
        // Rule 1: Check if value is greater than 10
        auto rule1 = std::make_shared<Rule>();
        rule1->id = 1;
        rule1->name = "check-value-rule";
        rule1->expression = "value > 10";
        rule1->action = "result.success = true";
        workflow.rules.push_back(rule1);
        
        // Rule 2: Double the value if first rule succeeded
        auto rule2 = std::make_shared<Rule>();
        rule2->id = 2;
        rule2->name = "double-value-rule";
        rule2->expression = "context.getResult(\"check-value-rule\").success";
        rule2->dependsOnRuleName = "check-value-rule";
        rule2->action = "result.success = true; result.doubledValue = value * 2";
        workflow.rules.push_back(rule2);
        
        // Compile workflow
        workflow.compile(engine);
        
        // Execute workflow with coroutine
        std::vector<RuleParameter> workflow_params = {
            {"value", 15}
        };
        
        // Use existing coExecuteWorkflow function
        auto workflow_task = coExecuteWorkflow(workflow, engine, workflow_params, 2);
        auto workflow_results = workflow_task.get();
        
        std::cout << "   Workflow results:\n";
        for (const auto& result : workflow_results) {
            std::cout << "     Rule: " << result.ruleName 
                      << ", Success: " << (result.success ? "true" : "false");
            std::cout << "\n";
        }
        
        std::cout << "\nCoroutine example completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}