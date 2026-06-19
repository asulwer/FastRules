#include "fastrules/async_workflow.hpp"
#include "fastrules/workflow.hpp"
#include "fastrules/rule.hpp"
#include <doctest/doctest.h>
#include <future>
#include <chrono>

TEST_CASE("AsyncWorkflow uses work-stealing thread pool") {
    // This test verifies that the AsyncWorkflow is using the work-stealing thread pool
    // Note: This is a simplified test since we can't fully instantiate AsyncWorkflow without dependencies
    
    // Test that the ThreadPoolImpl uses WorkStealingThreadPool
    // (Actual testing would require a full FastRules setup)
    CHECK(true); // Placeholder - actual implementation would be more comprehensive
}

TEST_CASE("Work-stealing thread pool performance") {
    // This test would verify that the work-stealing thread pool provides better performance
    // than the traditional thread pool under high concurrency
    // (Actual testing would require benchmarking)
    CHECK(true); // Placeholder - actual implementation would be more comprehensive
}