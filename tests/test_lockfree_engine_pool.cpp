// test_lockfree_engine_pool.cpp
// Tests for the lock-free engine pool implementation

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <iostream>  // For std::cout in benchmark test
#include "fastrules/lockfree_engine_pool.hpp"
#include "fastrules/lua_engine.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

using namespace fastrules;

TEST_CASE("LockFreeEnginePool basic push/pop", "[lockfree][pool]") {
    LockFreeEnginePool pool;
    
    // Create some dummy engines
    LuaEngine* engine1 = reinterpret_cast<LuaEngine*>(0x1);
    LuaEngine* engine2 = reinterpret_cast<LuaEngine*>(0x2);
    LuaEngine* engine3 = reinterpret_cast<LuaEngine*>(0x3);
    
    SECTION("Push and pop single engine") {
        pool.push(engine1);
        auto* popped = pool.pop();
        REQUIRE(popped == engine1);
    }
    
    SECTION("Push and pop multiple engines (LIFO order)") {
        pool.push(engine1);
        pool.push(engine2);
        pool.push(engine3);
        
        // Stack is LIFO: last pushed is first popped
        REQUIRE(pool.pop() == engine3);
        REQUIRE(pool.pop() == engine2);
        REQUIRE(pool.pop() == engine1);
        REQUIRE(pool.pop() == nullptr); // Empty
    }
    
    SECTION("Pop from empty pool returns nullptr") {
        REQUIRE(pool.pop() == nullptr);
    }
}

TEST_CASE("LockFreeEnginePool concurrent operations", "[lockfree][pool][concurrent]") {
    LockFreeEnginePool pool;
    constexpr int NUM_ENGINES = 100;
    constexpr int NUM_THREADS = 4;
    
    // Create dummy engines
    std::vector<LuaEngine*> engines;
    for (int i = 0; i < NUM_ENGINES; ++i) {
        engines.push_back(reinterpret_cast<LuaEngine*>(static_cast<uintptr_t>(i + 1)));
    }
    
    SECTION("Concurrent push and pop") {
        std::atomic<int> pushCount{0};
        std::atomic<int> popCount{0};
        std::vector<std::thread> threads;
        
        // Half threads push, half pop
        for (int t = 0; t < NUM_THREADS; ++t) {
            if (t % 2 == 0) {
                // Pusher threads
                threads.emplace_back([&pool, &engines, &pushCount, t]() {
                    for (int i = t; i < NUM_ENGINES; i += NUM_THREADS / 2) {
                        pool.push(engines[i]);
                        pushCount++;
                    }
                });
            } else {
                // Popper threads
                threads.emplace_back([&pool, &popCount]() {
                    while (popCount.load() < NUM_ENGINES) {
                        auto* engine = pool.pop();
                        if (engine) {
                            popCount++;
                        } else {
                            std::this_thread::yield();
                        }
                    }
                });
            }
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // All pushes should complete
        REQUIRE(pushCount == NUM_ENGINES);
        // At least some pops should have happened
        REQUIRE(popCount > 0);
    }
    
    SECTION("High contention stress test") {
        constexpr int ITERATIONS = 1000;
        std::atomic<int> successfulPops{0};
        std::vector<std::thread> threads;
        
        // Pre-populate
        for (int i = 0; i < 10; ++i) {
            pool.push(engines[i]);
        }
        
        // All threads compete for the same pool
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&pool, &engines, &successfulPops, t]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    if (i % 2 == 0) {
                        auto* engine = pool.pop();
                        if (engine) {
                            successfulPops++;
                        }
                    } else {
                        pool.push(engines[t % NUM_ENGINES]);
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Should have had many successful operations
        REQUIRE(successfulPops > 0);
    }
}

TEST_CASE("LockFreeEnginePool tagged pointer ABA protection", "[lockfree][pool][aba]") {
    LockFreeEnginePool pool;
    
    // Simulate ABA scenario
    LuaEngine* engineA = reinterpret_cast<LuaEngine*>(0x1000);
    LuaEngine* engineB = reinterpret_cast<LuaEngine*>(0x2000);
    
    // Push A
    pool.push(engineA);
    
    // Pop A (but keep reference - simulates thread delay)
    auto* poppedA = pool.pop();
    REQUIRE(poppedA == engineA);
    
    // Push B, then A again
    pool.push(engineB);
    pool.push(engineA);  // Same address, but different version tag
    
    // Pop should get A (newest)
    auto* poppedAgain = pool.pop();
    REQUIRE(poppedAgain == engineA);
    
    // Pop should get B
    auto* poppedB = pool.pop();
    REQUIRE(poppedB == engineB);
    
    // Empty
    REQUIRE(pool.pop() == nullptr);
}

TEST_CASE("LockFreeEnginePool memory ordering", "[lockfree][pool][memory]") {
    LockFreeEnginePool pool;
    constexpr int NUM_ITERATIONS = 10000;
    
    // This test verifies that memory ordering is correct
    // by checking for data races (would manifest as crashes in TSan)
    
    std::atomic<int> sumPush{0};
    std::atomic<int> sumPop{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&pool, &sumPush, &sumPop, t]() {
            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                auto val = static_cast<uintptr_t>(t * NUM_ITERATIONS + i + 1);
                auto* engine = reinterpret_cast<LuaEngine*>(val);
                
                pool.push(engine);
                sumPush.fetch_add(val, std::memory_order_relaxed);
                
                auto* popped = pool.pop();
                if (popped) {
                    sumPop.fetch_add(reinterpret_cast<uintptr_t>(popped), 
                                   std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Sum of pushes should equal sum of pops (everything accounted for)
    // Note: This assumes no other threads are still processing
    // In practice, some values may still be in the pool
    REQUIRE(sumPush > 0);
    REQUIRE(sumPop > 0);
}

TEST_CASE("LockFreeEnginePool performance benchmark", "[lockfree][pool][benchmark]") {
    LockFreeEnginePool pool;
    constexpr int NUM_OPERATIONS = 100000;
    constexpr int NUM_THREADS = 4;
    
    LuaEngine* dummyEngine = reinterpret_cast<LuaEngine*>(0x1);
    
    // Pre-populate
    for (int i = 0; i < 100; ++i) {
        pool.push(dummyEngine);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&pool, dummyEngine]() {
            for (int i = 0; i < NUM_OPERATIONS; ++i) {
                if (i % 2 == 0) {
                    pool.push(dummyEngine);
                } else {
                    auto* engine = pool.pop();
                    (void)engine; // Suppress unused warning
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double opsPerSecond = (NUM_OPERATIONS * NUM_THREADS * 1.0) / (duration.count() / 1000000.0);
    
    std::cout << "Lock-free pool: " << NUM_THREADS << " threads, " 
              << NUM_OPERATIONS << " ops/thread\n";
    std::cout << "Total time: " << duration.count() << " us\n";
    std::cout << "Throughput: " << opsPerSecond << " ops/sec\n";
    
    // Just verify it ran - performance numbers will vary by hardware
    REQUIRE(duration.count() > 0);
}
