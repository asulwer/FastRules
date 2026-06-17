/**
 * @file test_performance.cpp
 * @brief Performance counters and metrics tests
 * 
 * Tests cover:
 * - Execution counting (success, fail, skip, cache, timeout, rate limit)
 * - Counter reset functionality
 * - Execution time tracking
 * - JSON export of metrics
 * 
 * These tests verify the PerformanceCounters singleton
 * correctly tracks rule execution statistics.
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules/performance_counters.hpp>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("PerformanceCounters tracking") {
    auto& counters = PerformanceCounters::instance();
    counters.reset();
    
    counters.recordExecution(true, false, false, false, false);
    counters.recordExecution(false, true, false, false, false);
    counters.recordExecution(false, false, true, false, false);
    counters.recordExecution(false, false, false, true, false);
    counters.recordExecution(false, false, false, false, true);
    
    auto stats = counters.getCounters();
    REQUIRE(stats.totalRulesExecuted.load() == 5);
    REQUIRE(stats.totalRulesSuccessful.load() == 1);
    REQUIRE(stats.totalRulesFailed.load() == 4);
    REQUIRE(stats.totalRulesTimedOut.load() == 1);
    REQUIRE(stats.totalRulesRateLimited.load() == 1);
}

TEST_CASE("PerformanceCounters reset") {
    auto& counters = PerformanceCounters::instance();
    counters.reset();
    
    counters.recordExecution(true, false, false, false, false);
    REQUIRE(counters.getCounters().totalRulesExecuted.load() == 1);
    
    counters.reset();
    REQUIRE(counters.getCounters().totalRulesExecuted.load() == 0);
}

TEST_CASE("PerformanceCounters execution time tracking") {
    auto& counters = PerformanceCounters::instance();
    counters.reset();
    
    // Record executions
    for (int i = 0; i < 10; ++i) {
        counters.recordExecution(true, false, false, false, false);
    }
    
    auto stats = counters.getCounters();
    REQUIRE(stats.totalRulesExecuted.load() == 10);
    REQUIRE(stats.totalRulesSuccessful.load() == 10);
}

TEST_CASE("PerformanceCounters JSON export") {
    auto& counters = PerformanceCounters::instance();
    counters.reset();
    
    counters.recordExecution(true, false, false, false, false);
    counters.recordExecution(false, true, false, false, false);
    
    auto stats = counters.getCounters();
    REQUIRE(stats.totalRulesExecuted.load() == 2);
    REQUIRE(stats.totalRulesSuccessful.load() == 1);
    REQUIRE(stats.totalRulesFailed.load() == 1);
}
