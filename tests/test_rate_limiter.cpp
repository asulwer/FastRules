/**
 * @file test_rate_limiter.cpp
 * @brief Rate limiter tests
 * 
 * Tests cover:
 * - Basic throttling (sliding window)
 * - Window reset after time period
 * - Multiple rule configurations
 * 
 * These tests verify the sliding window rate limiter
 * correctly tracks and limits rule execution.
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules/rate_limiter.hpp>
#include <thread>
#include <chrono>

using namespace fastrules;

TEST_CASE("RateLimiter basic throttling") {
    RateLimiter limiter;
    limiter.configure({"test-rule", 3, 0, 0});
    
    // First 3 calls should pass
    REQUIRE(limiter.isAllowed("test-rule"));
    REQUIRE(limiter.isAllowed("test-rule"));
    REQUIRE(limiter.isAllowed("test-rule"));
    
    // 4th call should fail (limit reached)
    REQUIRE_FALSE(limiter.isAllowed("test-rule"));
}

TEST_CASE("RateLimiter window reset") {
    RateLimiter limiter;
    limiter.configure({"test-rule", 1, 0, 0});
    
    REQUIRE(limiter.isAllowed("test-rule"));
    REQUIRE_FALSE(limiter.isAllowed("test-rule"));
    
    // Wait for window to reset
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    REQUIRE(limiter.isAllowed("test-rule"));
}

TEST_CASE("RateLimiter multiple rules") {
    RateLimiter limiter;
    limiter.configure({"rule-a", 2, 0, 0});
    limiter.configure({"rule-b", 3, 0, 0});
    
    // Each rule has its own limit
    REQUIRE(limiter.isAllowed("rule-a"));
    REQUIRE(limiter.isAllowed("rule-a"));
    REQUIRE_FALSE(limiter.isAllowed("rule-a"));  // Exceeded
    
    // rule-b still has capacity
    REQUIRE(limiter.isAllowed("rule-b"));
    REQUIRE(limiter.isAllowed("rule-b"));
    REQUIRE(limiter.isAllowed("rule-b"));
    REQUIRE_FALSE(limiter.isAllowed("rule-b"));  // Exceeded
}

TEST_CASE("RateLimiter per minute limit") {
    RateLimiter limiter;
    limiter.configure({"test-rule", 0, 2, 0});  // 2 per minute
    
    REQUIRE(limiter.isAllowed("test-rule"));
    REQUIRE(limiter.isAllowed("test-rule"));
    REQUIRE_FALSE(limiter.isAllowed("test-rule"));
}

TEST_CASE("RateLimiter burst size") {
    RateLimiter limiter;
    limiter.configure({"test-rule", 1, 0, 5});  // 1 per second, burst of 5
    
    // Should allow burst of 5
    for (int i = 0; i < 5; ++i) {
        REQUIRE(limiter.isAllowed("test-rule"));
    }
    REQUIRE_FALSE(limiter.isAllowed("test-rule"));
}
