#include "fastrules/memory_pool.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <vector>
#include <future>
#include <memory>

using namespace fastrules;

// Test structure
struct TestObject {
    int value;
    std::string name;
    
    TestObject(int v = 0, const std::string& n = "") : value(v), name(n) {}
};

TEST_CASE("MemoryPool advanced functionality") {
    MemoryPool<int> pool(10);
    
    // Test pool initialization
    auto stats = pool.getStats();
    CHECK(stats.first == 0); // Available objects (none pre-allocated)
    CHECK(stats.second == 0); // Total objects (none allocated yet)
    
    // Test acquiring objects
    std::vector<std::unique_ptr<int>> objects;
    for (int i = 0; i < 5; ++i) {
        auto obj = pool.acquire();
        REQUIRE(obj != nullptr);
        *obj = i;
        objects.push_back(std::move(obj));
    }
    
    // Check stats after acquiring
    stats = pool.getStats();
    CHECK(stats.first == 0); // Available objects (none in pool)
    CHECK(stats.second == 5); // Total objects allocated
    
    // Test releasing objects
    for (auto& obj : objects) {
        pool.release(std::move(obj));
    }
    
    // Check stats after releasing
    stats = pool.getStats();
    CHECK(stats.first == 5); // Available objects (now in pool)
    CHECK(stats.second == 5); // Total objects unchanged
    
    // Test reusing released objects
    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();
    CHECK(obj1 != nullptr);
    CHECK(obj2 != nullptr);
    
    pool.release(std::move(obj1));
    pool.release(std::move(obj2));
    
    stats = pool.getStats();
    CHECK(stats.first == 7); // Available objects (5 from before + 2 released)
    CHECK(stats.second == 7); // Total objects unchanged
}

TEST_CASE("MemoryPool thread safety") {
    MemoryPool<int> pool(100);
    
    // Test concurrent access from multiple threads
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&pool, i]() {
            std::vector<std::unique_ptr<int>> objects;
            
            // Acquire multiple objects
            for (int j = 0; j < 5; ++j) {
                auto obj = pool.acquire();
                if (obj) {
                    *obj = i * 10 + j;
                    objects.push_back(std::move(obj));
                }
            }
            
            int count = static_cast<int>(objects.size());
            
            // Release all objects
            for (auto& obj : objects) {
                pool.release(std::move(obj));
            }
            
            return count;
        }));
    }
    
    // Check results
    int totalAcquired = 0;
    for (auto& future : futures) {
        totalAcquired += future.get();
    }
    
    // All objects should have been acquired and released properly
    CHECK(totalAcquired == 50); // 10 threads * 5 objects each
    
    // Check final stats
    auto stats = pool.getStats();
    CHECK(stats.first == 50); // All objects should be available in pool
    CHECK(stats.second == 50); // Total should remain the same
}

TEST_CASE("MemoryPool stress test") {
    MemoryPool<int> pool(1000);
    
    // Acquire all objects
    std::vector<std::unique_ptr<int>> objects;
    for (int i = 0; i < 1000; ++i) {
        auto obj = pool.acquire();
        REQUIRE(obj != nullptr);
        *obj = i;
        objects.push_back(std::move(obj));
    }
    
    auto stats = pool.getStats();
    CHECK(stats.first == 0); // No objects available in pool
    CHECK(stats.second == 1000); // Total objects
    
    // Release all objects
    for (auto& obj : objects) {
        pool.release(std::move(obj));
    }
    
    stats = pool.getStats();
    CHECK(stats.first == 1000); // All objects back in pool
    CHECK(stats.second == 1000); // Total objects unchanged
    
    // Acquire them again to verify they're reusable
    for (int i = 0; i < 1000; ++i) {
        auto obj = pool.acquire();
        REQUIRE(obj != nullptr);
        pool.release(std::move(obj));
    }
    
    stats = pool.getStats();
    CHECK(stats.first == 1000); // All objects back in pool
}

TEST_CASE("VectorPool advanced functionality") {
    VectorPool<int> pool(50, 10);
    
    // Test acquiring vectors
    auto vec1 = pool.acquire();
    REQUIRE(vec1 != nullptr);
    CHECK(vec1->empty());
    CHECK(vec1->capacity() >= 10);
    
    // Add some data
    for (int i = 0; i < 15; ++i) {
        vec1->push_back(i);
    }
    CHECK(vec1->size() == 15);
    
    // Test releasing vector back to pool
    pool.release(std::move(vec1));
    
    // Acquire again and check it's been reset
    auto vec2 = pool.acquire();
    REQUIRE(vec2 != nullptr);
    CHECK(vec2->empty()); // Should be reset
    CHECK(vec2->capacity() >= 10); // Should maintain capacity
    
    pool.release(std::move(vec2));
    
    // Check pool stats
    auto stats = pool.getStats();
    CHECK(stats.first == 2); // Available vectors (2 released back to pool)
    CHECK(stats.second == 2); // Total vectors (2 allocated total)
}

TEST_CASE("VectorPool thread safety") {
    VectorPool<std::string> pool(100, 5);
    
    // Test concurrent access
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [&pool, i]() {
            std::vector<std::unique_ptr<std::vector<std::string>>> vectors;
            
            // Acquire multiple vectors
            for (int j = 0; j < 10; ++j) {
                auto vec = pool.acquire();
                if (vec) {
                    // Add some data
                    for (int k = 0; k < 5; ++k) {
                        vec->push_back("thread_" + std::to_string(i) + "_vector_" + std::to_string(j) + "_item_" + std::to_string(k));
                    }
                    vectors.push_back(std::move(vec));
                }
            }
            
            // Release all vectors
            for (auto& vec : vectors) {
                pool.release(std::move(vec));
            }
            
            return true;
        }));
    }
    
    // Wait for all threads
    for (auto& future : futures) {
        REQUIRE(future.get() == true);
    }
    
    // Check final stats
    auto stats = pool.getStats();
    CHECK(stats.first == 50); // All vectors should be available (5 threads * 10 vectors each)
    CHECK(stats.second == 50); // Total vectors unchanged
}

TEST_CASE("MemoryPool boundary conditions") {
    MemoryPool<int> pool(1);
    
    // Test pool with capacity of 1
    auto obj1 = pool.acquire();
    REQUIRE(obj1 != nullptr);
    
    auto obj2 = pool.acquire();
    REQUIRE(obj2 != nullptr); // Should succeed (creates new object)
    
    pool.release(std::move(obj1));
    pool.release(std::move(obj2));
    
    auto obj3 = pool.acquire();
    REQUIRE(obj3 != nullptr);
    CHECK(obj3 != obj1); // Should be a different object (implementation dependent)
    
    pool.release(std::move(obj3));
    
    // Test with zero capacity
    MemoryPool<double> zeroPool(0);
    auto zeroObj = zeroPool.acquire();
    REQUIRE(zeroObj != nullptr); // Should succeed (creates new object)
    
    zeroPool.release(std::move(zeroObj));
    
    auto stats = zeroPool.getStats();
    CHECK(stats.first == 1); // One object should be in pool
    CHECK(stats.second == 1); // One object total
}

TEST_CASE("MemoryPool performance") {
    MemoryPool<int> pool(10000);
    
    // Test bulk operations performance
    std::vector<std::unique_ptr<int>> objects;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Acquire many objects
    for (int i = 0; i < 5000; ++i) {
        auto obj = pool.acquire();
        if (obj) {
            *obj = i;
            objects.push_back(std::move(obj));
        }
    }
    
    // Release all objects
    for (auto& obj : objects) {
        pool.release(std::move(obj));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 10000 operations quickly
    CHECK(duration.count() < 1000); // 1 second should be more than enough
    
    // Verify all objects are back in pool
    auto stats = pool.getStats();
    CHECK(stats.first == 5000); // All objects available
    CHECK(stats.second == 5000); // Total objects unchanged
}

TEST_CASE("MemoryPool preallocation") {
    MemoryPool<int> pool(100);
    
    // Test preallocation
    pool.preallocate(10);
    
    auto stats = pool.getStats();
    CHECK(stats.first == 10); // 10 objects pre-allocated in pool
    CHECK(stats.second == 10); // 10 objects total
    
    // Test that we can acquire pre-allocated objects
    auto obj = pool.acquire();
    REQUIRE(obj != nullptr);
    
    stats = pool.getStats();
    CHECK(stats.first == 9); // One object removed from pool
    CHECK(stats.second == 10); // Total unchanged
    
    pool.release(std::move(obj));
    
    stats = pool.getStats();
    CHECK(stats.first == 10); // Object returned to pool
    CHECK(stats.second == 10); // Total unchanged
}

TEST_CASE("VectorPool preallocation") {
    VectorPool<std::string> pool(50, 5);
    
    // Test preallocation
    pool.preallocate(10);
    
    auto stats = pool.getStats();
    CHECK(stats.first == 10); // 10 vectors pre-allocated in pool
    CHECK(stats.second == 10); // 10 vectors total
    
    // Test that we can acquire pre-allocated vectors
    auto vec = pool.acquire();
    REQUIRE(vec != nullptr);
    CHECK(vec->empty()); // Should be empty
    
    stats = pool.getStats();
    CHECK(stats.first == 9); // One vector removed from pool
    CHECK(stats.second == 10); // Total unchanged
    
    pool.release(std::move(vec));
    
    stats = pool.getStats();
    CHECK(stats.first == 10); // Vector returned to pool
    CHECK(stats.second == 10); // Total unchanged
}

TEST_CASE("MemoryPool clear functionality") {
    MemoryPool<int> pool(10);
    
    // Acquire and release some objects
    for (int i = 0; i < 5; ++i) {
        auto obj = pool.acquire();
        *obj = i;
        pool.release(std::move(obj));
    }
    
    auto stats = pool.getStats();
    CHECK(stats.first == 5); // 5 objects in pool
    
    // Clear the pool
    pool.clear();
    
    stats = pool.getStats();
    CHECK(stats.first == 0); // No objects in pool after clear
    CHECK(stats.second == 5); // Total unchanged (objects still allocated)
    
    // Test that we can still acquire objects after clear
    auto obj = pool.acquire();
    REQUIRE(obj != nullptr);
    CHECK(stats.second == 6); // One more object allocated
}

TEST_CASE("MemoryPool edge cases") {
    // Test with very large pool size
    MemoryPool<int> largePool(1000000);
    
    // Test that we can acquire objects from large pool
    auto obj = largePool.acquire();
    REQUIRE(obj != nullptr);
    
    largePool.release(std::move(obj));
    
    auto stats = largePool.getStats();
    CHECK(stats.first == 1); // One object in pool
    CHECK(stats.second == 1); // One object total
    
    // Test with very small initial allocation
    MemoryPool<double> smallPool(1);
    smallPool.preallocate(1);
    
    stats = smallPool.getStats();
    CHECK(stats.first == 1); // One object pre-allocated
    CHECK(stats.second == 1); // One object total
}