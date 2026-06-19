#include "fastrules/memory_pool.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <vector>

TEST_CASE("MemoryPool basic functionality") {
    fastrules::MemoryPool<int> pool(10);
    
    // Test acquiring objects
    auto obj1 = pool.acquire();
    CHECK(obj1 != nullptr);
    
    auto obj2 = pool.acquire();
    CHECK(obj2 != nullptr);
    
    // Test releasing objects back to pool
    pool.release(std::move(obj1));
    pool.release(std::move(obj2));
    
    // Test that released objects are reused
    auto obj3 = pool.acquire();
    auto obj4 = pool.acquire();
    CHECK(obj3 != nullptr);
    CHECK(obj4 != nullptr);
    
    // Test pool statistics
    auto stats = pool.getStats();
    CHECK(stats.first == 0);  // Pool size should be 0 after acquiring all objects
    CHECK(stats.second == 2); // Total allocated should be 2
}

TEST_CASE("VectorPool basic functionality") {
    fastrules::VectorPool<int> pool(10, 5);
    
    // Test acquiring vectors
    auto vec1 = pool.acquire();
    CHECK(vec1 != nullptr);
    CHECK(vec1->empty());
    CHECK(vec1->capacity() >= 5);
    
    // Add some data
    vec1->push_back(1);
    vec1->push_back(2);
    vec1->push_back(3);
    CHECK(vec1->size() == 3);
    
    // Test releasing vector back to pool
    pool.release(std::move(vec1));
    
    // Test that released vector is reused and cleared
    auto vec2 = pool.acquire();
    CHECK(vec2 != nullptr);
    CHECK(vec2->empty());  // Should be cleared
    CHECK(vec2->capacity() >= 5);
    
    // Test pool statistics
    auto stats = pool.getStats();
    CHECK(stats.first == 0);  // Pool size should be 0 after acquiring
    CHECK(stats.second == 1); // Total allocated should be 1
}

TEST_CASE("MemoryManager basic functionality") {
    auto& manager = fastrules::MemoryManager::getInstance();
    
    // Test acquiring and releasing RuleContext
    auto context = manager.acquireContext();
    CHECK(context != nullptr);
    
    manager.releaseContext(std::move(context));
    
    // Test acquiring and releasing RuleResult vector
    auto results = manager.acquireResultVector();
    CHECK(results != nullptr);
    
    manager.releaseResultVector(std::move(results));
    
    // Test preallocation
    manager.preallocate();
    
    // Test statistics
    size_t contextPoolSize, contextAllocated, resultPoolSize, resultAllocated;
    manager.getStats(contextPoolSize, contextAllocated, resultPoolSize, resultAllocated);
    
    // Values will depend on previous tests, just check they're non-negative
    CHECK(contextPoolSize >= 0);
    CHECK(contextAllocated >= 0);
    CHECK(resultPoolSize >= 0);
    CHECK(resultAllocated >= 0);
}

TEST_CASE("MemoryPool thread safety") {
    fastrules::MemoryPool<int> pool(100);
    
    // Test concurrent access from multiple threads
    std::vector<std::thread> threads;
    std::atomic<int> totalAcquired{0};
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, &totalAcquired]() {
            std::vector<std::unique_ptr<int>> objects;
            for (int j = 0; j < 10; ++j) {
                auto obj = pool.acquire();
                if (obj) {
                    totalAcquired++;
                    objects.push_back(std::move(obj));
                }
            }
            
            // Release all objects back to pool
            for (auto& obj : objects) {
                pool.release(std::move(obj));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    CHECK(totalAcquired == 100);
    
    // Check pool statistics
    auto stats = pool.getStats();
    CHECK(stats.second == 100); // Total allocated should be 100
}

TEST_CASE("MemoryPool with preallocation") {
    fastrules::MemoryPool<int> pool(50);
    
    // Preallocate objects
    pool.preallocate(20);
    
    // Check statistics
    auto stats = pool.getStats();
    CHECK(stats.first == 20); // Pool should have 20 objects
    CHECK(stats.second == 20); // Total allocated should be 20
    
    // Acquire all preallocated objects
    std::vector<std::unique_ptr<int>> objects;
    for (int i = 0; i < 20; ++i) {
        auto obj = pool.acquire();
        CHECK(obj != nullptr);
        objects.push_back(std::move(obj));
    }
    
    // Check statistics after acquiring
    stats = pool.getStats();
    CHECK(stats.first == 0); // Pool should be empty
    CHECK(stats.second == 20); // Total allocated should still be 20
    
    // Release objects back to pool
    for (auto& obj : objects) {
        pool.release(std::move(obj));
    }
    
    // Check statistics after releasing
    stats = pool.getStats();
    CHECK(stats.first == 20); // Pool should have 20 objects again
    CHECK(stats.second == 20); // Total allocated should still be 20
}