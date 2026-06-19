/**
 * @file memory_pool_example.cpp
 * @brief Example demonstrating memory pooling with FastRules
 * 
 * This example shows how to use memory pools to reduce allocation
 * overhead for frequently used objects.
 */

#include "fastrules/memory_pool.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

using namespace fastrules;

// Simple benchmark helper
class Benchmark {
private:
    std::chrono::high_resolution_clock::time_point start_;

public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    auto stop() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    }
};

int main() {
    std::cout << "FastRules Memory Pool Example\n";
    std::cout << "=============================\n\n";
    
    try {
        // Get memory manager instance
        auto& manager = MemoryManager::getInstance();
        
        // Preallocate objects
        manager.preallocate();
        
        // Example 1: Basic memory pool usage
        std::cout << "1. Basic memory pool usage:\n";
        
        // Acquire objects from pool
        auto context1 = manager.acquireContext();
        auto context2 = manager.acquireContext();
        
        std::cout << "   Acquired 2 RuleContext objects from pool\n";
        
        // Release objects back to pool
        manager.releaseContext(std::move(context1));
        manager.releaseContext(std::move(context2));
        
        std::cout << "   Released 2 RuleContext objects back to pool\n\n";
        
        // Example 2: RuleResult vector pooling
        std::cout << "2. RuleResult vector pooling:\n";
        
        // Acquire vector from pool
        auto results1 = manager.acquireResultVector();
        results1->reserve(10);  // Reserve space
        
        // Add some results
        RuleResult result1;
        result1.ruleName = "test-rule-1";
        result1.success = true;
        results1->push_back(result1);
        
        auto results2 = manager.acquireResultVector();
        results2->reserve(5);
        
        std::cout << "   Acquired 2 RuleResult vectors from pool\n";
        std::cout << "   Added data to vectors\n";
        
        // Release vectors back to pool
        manager.releaseResultVector(std::move(results1));
        manager.releaseResultVector(std::move(results2));
        
        std::cout << "   Released 2 RuleResult vectors back to pool\n\n";
        
        // Example 3: Performance comparison
        std::cout << "3. Performance comparison (10000 iterations):\n";
        
        Benchmark bench;
        
        // Test normal allocation
        bench.start();
        for (int i = 0; i < 10000; ++i) {
            auto context = std::make_unique<RuleContext>();
            // Use context...
            context.reset();  // Explicitly destroy
        }
        auto normalTime = bench.stop();
        
        // Test pooled allocation
        bench.start();
        for (int i = 0; i < 10000; ++i) {
            auto context = manager.acquireContext();
            // Use context...
            manager.releaseContext(std::move(context));
        }
        auto pooledTime = bench.stop();
        
        std::cout << "   Normal allocation: " << normalTime.count() << " microseconds\n";
        std::cout << "   Pooled allocation: " << pooledTime.count() << " microseconds\n";
        std::cout << "   Performance improvement: " 
                  << (normalTime.count() > 0 ? 
                      (static_cast<double>(normalTime.count() - pooledTime.count()) / normalTime.count() * 100) : 0)
                  << "%\n\n";
        
        // Example 4: Memory manager statistics
        std::cout << "4. Memory manager statistics:\n";
        
        size_t contextPoolSize, contextAllocated, resultPoolSize, resultAllocated;
        manager.getStats(contextPoolSize, contextAllocated, resultPoolSize, resultAllocated);
        
        std::cout << "   RuleContext pool size: " << contextPoolSize << "\n";
        std::cout << "   RuleContext total allocated: " << contextAllocated << "\n";
        std::cout << "   RuleResult pool size: " << resultPoolSize << "\n";
        std::cout << "   RuleResult total allocated: " << resultAllocated << "\n\n";
        
        // Example 5: Generic memory pool usage
        std::cout << "5. Generic memory pool usage:\n";
        
        // Create a pool for integers
        MemoryPool<int> intPool(100);
        
        // Preallocate some integers
        intPool.preallocate(50);
        
        // Acquire and use integers
        std::vector<std::unique_ptr<int>> integers;
        for (int i = 0; i < 10; ++i) {
            auto intPtr = intPool.acquire();
            *intPtr = i * i;  // Store square of i
            integers.push_back(std::move(intPtr));
        }
        
        std::cout << "   Acquired 10 integers from pool: ";
        for (const auto& intPtr : integers) {
            std::cout << *intPtr << " ";
        }
        std::cout << "\n";
        
        // Release integers back to pool
        for (auto& intPtr : integers) {
            intPool.release(std::move(intPtr));
        }
        
        auto intStats = intPool.getStats();
        std::cout << "   Integer pool size: " << intStats.first << "\n";
        std::cout << "   Integer total allocated: " << intStats.second << "\n\n";
        
        std::cout << "Memory pool example completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}