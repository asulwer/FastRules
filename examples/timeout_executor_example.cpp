/**
 * @file timeout_executor_example.cpp
 * @brief Example demonstrating timeout enforcement with FastRules
 * 
 * This example shows how to use timeout executors to prevent
 * infinite loops and long-running rules.
 */

#include "fastrules/timeout_executor.hpp"
#include <iostream>
#include <vector>
#include <chrono>

using namespace fastrules;

int main() {
    std::cout << "FastRules Timeout Executor Example\n";
    std::cout << "==================================\n\n";
    
    try {
        // Example 1: Basic timeout execution
        std::cout << "1. Basic timeout execution:\n";
        
        using namespace std::chrono_literals;
        TimeoutExecutor executor(100ms);
        
        // Test successful execution within timeout
        try {
            auto result = executor.executeWithTimeout([]() {
                std::this_thread::sleep_for(50ms);
                return 42;
            });
            std::cout << "   ✓ Successful execution: " << result << "\n";
        } catch (const RuleTimeoutException& e) {
            std::cout << "   ✗ Timeout occurred: " << e.what() << "\n";
        }
        
        // Test timeout exception
        try {
            auto result = executor.executeWithTimeout([]() {
                std::this_thread::sleep_for(200ms);
                return 42;
            });
            std::cout << "   ✓ Successful execution: " << result << "\n";
        } catch (const RuleTimeoutException& e) {
            std::cout << "   ✗ Timeout occurred (as expected): " << e.what() << "\n";
        }
        std::cout << "\n";
        
        // Example 2: RuleExecutor with layered timeout protection
        std::cout << "2. RuleExecutor with layered timeout protection:\n";
        
        RuleExecutor ruleExecutor(150ms);
        
        // Test successful rule execution
        try {
            auto result = ruleExecutor.execute([]() {
                // Simulate some rule processing
                std::this_thread::sleep_for(50ms);
                return "Rule executed successfully";
            });
            std::cout << "   ✓ Rule execution: " << result << "\n";
        } catch (const RuleTimeoutException& e) {
            std::cout << "   ✗ Rule timeout: " << e.what() << "\n";
        }
        
        // Test rule that times out
        try {
            auto result = ruleExecutor.execute([]() {
                // Simulate a slow rule
                std::this_thread::sleep_for(300ms);
                return "This should not be reached";
            });
            std::cout << "   ✓ Rule execution: " << result << "\n";
        } catch (const RuleTimeoutException& e) {
            std::cout << "   ✗ Rule timeout (as expected): " << e.what() << "\n";
        }
        std::cout << "\n";
        
        // Example 3: Different timeout configurations
        std::cout << "3. Different timeout configurations:\n";
        
        std::vector<std::pair<std::string, std::chrono::milliseconds>> timeouts = {
            {"Very short", 1ms},
            {"Short", 10ms},
            {"Medium", 100ms},
            {"Long", 1000ms}
        };
        
        for (const auto& [name, timeout] : timeouts) {
            TimeoutExecutor timeoutExecutor(timeout);
            try {
                auto result = timeoutExecutor.executeWithTimeout([timeout]() {
                    std::this_thread::sleep_for(timeout / 2);  // Sleep for half the timeout
                    return timeout.count();
                });
                std::cout << "   ✓ " << name << " timeout (" << timeout.count() << "ms): " << result << "ms\n";
            } catch (const RuleTimeoutException& e) {
                std::cout << "   ✗ " << name << " timeout (" << timeout.count() << "ms): " << e.what() << "\n";
            }
        }
        std::cout << "\n";
        
        // Example 4: TimeoutGuard usage
        std::cout << "4. TimeoutGuard usage:\n";
        
        {
            TimeoutGuard guard(100ms);
            std::this_thread::sleep_for(50ms);
            std::cout << "   ✓ Short operation completed within guard\n";
        }
        
        {
            TimeoutGuard guard(50ms);
            std::this_thread::sleep_for(100ms);
            // Note: The guard doesn't actively terminate the thread in this simple implementation
            std::cout << "   ✓ Long operation completed (guard timeout detection not fully implemented)\n";
        }
        std::cout << "\n";
        
        // Example 5: Exception handling with timeouts
        std::cout << "5. Exception handling with timeouts:\n";
        
        TimeoutExecutor exceptionExecutor(100ms);
        
        // Test timeout with exception
        try {
            exceptionExecutor.executeWithTimeout([]() {
                std::this_thread::sleep_for(200ms);
                throw std::runtime_error("This exception should not be thrown");
                return 42;
            });
            std::cout << "   ✓ Execution completed\n";
        } catch (const RuleTimeoutException& e) {
            std::cout << "   ✗ Timeout occurred (as expected): " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cout << "   ✗ Other exception: " << e.what() << "\n";
        }
        
        // Test normal exception within timeout
        try {
            exceptionExecutor.executeWithTimeout([]() {
                std::this_thread::sleep_for(10ms);
                throw std::runtime_error("Test exception");
                return 42;
            });
            std::cout << "   ✓ Execution completed\n";
        } catch (const RuleTimeoutException& e) {
            std::cout << "   ✗ Timeout occurred: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cout << "   ✗ Other exception (as expected): " << e.what() << "\n";
        }
        std::cout << "\n";
        
        // Example 6: Performance testing
        std::cout << "6. Performance testing:\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Execute many quick operations
        for (int i = 0; i < 100; ++i) {
            TimeoutExecutor quickExecutor(1ms);
            try {
                auto result = quickExecutor.executeWithTimeout([]() {
                    return 42;
                });
                // Don't print each result to avoid spam
            } catch (...) {
                // Ignore exceptions for this test
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "   Executed 100 quick timeout operations in " << duration.count() << " milliseconds\n\n";
        
        std::cout << "Timeout executor example completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}