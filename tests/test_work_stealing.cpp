#include "fastrules/work_stealing_thread_pool.hpp"
#include "fastrules/simple_work_stealing_queue.hpp"
#include <doctest/doctest.h>
#include <future>
#include <chrono>

TEST_CASE("Work-stealing thread pool basic functionality") {
    // Create a work-stealing thread pool with 4 threads
    fastrules::WorkStealingThreadPool pool(4);
    
    // Test enqueuing a simple task
    auto future = pool.enqueue([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });
    
    // Get the result
    int result = future.get();
    CHECK(result == 42);
}

TEST_CASE("Work-stealing thread pool multiple tasks") {
    // Create a work-stealing thread pool with 4 threads
    fastrules::WorkStealingThreadPool pool(4);
    
    // Test multiple tasks
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return i * i;
        }));
    }
    
    // Collect results
    for (int i = 0; i < 10; ++i) {
        CHECK(futures[i].get() == i * i);
    }
}

TEST_CASE("Work-stealing thread pool with exceptions") {
    // Create a work-stealing thread pool with 2 threads
    fastrules::WorkStealingThreadPool pool(2);
    
    // Test task that throws an exception
    auto future = pool.enqueue([]() -> int {
        throw std::runtime_error("Test exception");
    });
    
    // Check that the exception is properly propagated
    CHECK_THROWS_AS(future.get(), std::runtime_error);
}

TEST_CASE("Simple work-stealing queue basic functionality") {
    // Create a simple work-stealing queue
    fastrules::SimpleWorkStealingQueue<int> queue;
    
    // Test push and pop
    queue.push(42);
    auto item = queue.pop();
    REQUIRE(item.has_value());
    CHECK(item.value() == 42);
    
    // Test empty queue
    CHECK(!queue.pop().has_value());
    CHECK(queue.empty());
}

TEST_CASE("Simple work-stealing queue steal functionality") {
    // Create a simple work-stealing queue
    fastrules::SimpleWorkStealingQueue<int> queue;
    
    // Add multiple items
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    // Pop from owner (LIFO)
    auto item1 = queue.pop();
    REQUIRE(item1.has_value());
    CHECK(item1.value() == 3);  // Last in, first out
    
    // Steal from other thread (FIFO)
    auto item2 = queue.steal();
    REQUIRE(item2.has_value());
    CHECK(item2.value() == 1);  // First in, first out
    
    // Pop remaining item
    auto item3 = queue.pop();
    REQUIRE(item3.has_value());
    CHECK(item3.value() == 2);
}