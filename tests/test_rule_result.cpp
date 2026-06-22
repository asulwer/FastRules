#include "fastrules/rule_result.hpp"
#include "fastrules/rule_context.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <chrono>

using namespace fastrules;

TEST_CASE("RuleResult basic functionality") {
    RuleResult result;
    
    // Test default values
    CHECK(result.ruleName.empty());
    CHECK(result.ruleId == 0);
    CHECK_FALSE(result.success);
    CHECK_FALSE(result.skipped);
    CHECK_FALSE(result.exception.has_value());
    
    // Test setting values
    result.ruleName = "test_rule";
    result.ruleId = 42;
    result.success = true;
    
    CHECK(result.ruleName == "test_rule");
    CHECK(result.ruleId == 42);
    CHECK(result.success);
    CHECK(result.isSuccess());
    
    // Test duration calculation
    result.executedAt = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    result.completedAt = std::chrono::steady_clock::now();
    
    auto duration = result.duration();
    CHECK(duration.count() > 0);
}

TEST_CASE("RuleResult isSuccess method") {
    RuleResult result;
    
    // Test success case
    result.success = true;
    CHECK(result.isSuccess());
    
    // Test failure when exception present
    result.exception = RuleException("Test exception");
    CHECK_FALSE(result.isSuccess());
    result.exception.reset();
    
    // Test failure when skipped
    result.skipped = true;
    CHECK_FALSE(result.isSuccess());
    result.skipped = false;
    
    // Test failure when not successful
    result.success = false;
    CHECK_FALSE(result.isSuccess());
}

TEST_CASE("RuleResult isFullySuccessful method") {
    RuleResult result;
    result.success = true;
    
    // Test with no children
    CHECK(result.isFullySuccessful());
    
    // Test with successful children
    RuleResult child1;
    child1.success = true;
    result.childResults.push_back(child1);
    
    RuleResult child2;
    child2.success = true;
    result.childResults.push_back(child2);
    
    CHECK(result.isFullySuccessful());
    
    // Test with failed child
    RuleResult child3;
    child3.success = false;
    result.childResults.push_back(child3);
    
    CHECK_FALSE(result.isFullySuccessful());
}

TEST_CASE("RuleException functionality") {
    // Test construction
    RuleException ex("Test error message");
    CHECK(std::string(ex.what()) == "Test error message");
    
    // Test copy constructor
    RuleException ex2(ex);
    CHECK(std::string(ex2.what()) == "Test error message");
    
    // Test move constructor
    RuleException ex3(std::move(ex2));
    CHECK(std::string(ex3.what()) == "Test error message");
}

TEST_CASE("AsyncRuleResult functionality") {
    AsyncRuleResult asyncResult;
    
    // Test default values
    CHECK_FALSE(asyncResult.isSuccess());
    
    // Test with successful result and no exception
    RuleResult result;
    result.success = true;
    asyncResult.result = result;
    CHECK(asyncResult.isSuccess());
    
    // Test with exception
    asyncResult.exception = std::make_exception_ptr(std::runtime_error("Test"));
    CHECK_FALSE(asyncResult.isSuccess());
}

TEST_CASE("RuleResult timing") {
    RuleResult result;
    
    // Test timing with actual execution
    auto start = std::chrono::steady_clock::now();
    result.executedAt = start;

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto end = std::chrono::steady_clock::now();
    result.completedAt = end;
    
    auto duration = result.duration();
    CHECK(duration.count() > 0);
    CHECK(duration.count() >= 10000); // Should be at least 10ms (10000 microseconds)
}

TEST_CASE("RuleResult child management") {
    RuleResult parent;
    parent.ruleName = "parent_rule";
    parent.ruleId = 1;
    parent.success = true;
    
    // Add child results
    RuleResult child1;
    child1.ruleName = "child_rule_1";
    child1.ruleId = 2;
    child1.success = true;
    
    RuleResult child2;
    child2.ruleName = "child_rule_2";
    child2.ruleId = 3;
    child2.success = false;
    
    RuleResult child3;
    child3.ruleName = "child_rule_3";
    child3.ruleId = 4;
    child3.success = true;
    
    parent.childResults.push_back(child1);
    parent.childResults.push_back(child2);
    parent.childResults.push_back(child3);
    
    // Test child count
    CHECK(parent.childResults.size() == 3);
    
    // Test isFullySuccessful (should be false because child2 failed)
    CHECK_FALSE(parent.isFullySuccessful());
    
    // Test getting specific children
    CHECK(parent.childResults[0].ruleName == "child_rule_1");
    CHECK(parent.childResults[0].success == true);
    CHECK(parent.childResults[1].ruleName == "child_rule_2");
    CHECK(parent.childResults[1].success == false);
    CHECK(parent.childResults[2].ruleName == "child_rule_3");
    CHECK(parent.childResults[2].success == true);
}

TEST_CASE("RuleResult exception handling") {
    RuleResult result;
    result.ruleName = "exception_test";
    result.ruleId = 100;
    result.success = false;
    
    // Test setting exception
    result.exception = RuleException("Something went wrong");
    CHECK(result.exception.has_value());
    CHECK(std::string(result.exception->what()) == "Something went wrong");
    
    // Test that isSuccess returns false when exception is present
    CHECK_FALSE(result.isSuccess());
    
    // Test clearing exception
    result.exception.reset();
    CHECK_FALSE(result.exception.has_value());
    
    // Test that isSuccess returns true when success is true and no exception
    result.success = true;
    CHECK(result.isSuccess());
}

TEST_CASE("RuleResult performance") {
    // Test creating and managing many RuleResult objects
    std::vector<RuleResult> results;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create many RuleResult objects
    for (int i = 0; i < 1000; ++i) {
        RuleResult result;
        result.ruleName = "performance_test_" + std::to_string(i);
        result.ruleId = i;
        result.success = (i % 2 == 0); // Alternate success/failure
        results.push_back(result);
    }
    
    // Modify some results
    for (int i = 0; i < 1000; ++i) {
        if (i % 10 == 5) {  // Change from 0 to 5 so index 0 is not affected
            results[i].skipped = true;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 1000 objects quickly
    CHECK(duration.count() < 1000); // 1 second should be more than enough
    
    // Verify some results
    CHECK(results[0].ruleName == "performance_test_0");
    CHECK(results[0].success == true);
    CHECK(results[0].skipped == false);  // Should still be false
    
    CHECK(results[10].ruleName == "performance_test_10");
    CHECK(results[10].success == true);
    CHECK(results[10].skipped == false);  // Index 10 should also be false now
}

TEST_CASE("RuleResult copy and move") {
    RuleResult original;
    original.ruleName = "copy_test";
    original.ruleId = 999;
    original.success = true;
    original.skipped = false;
    
    // Test copy constructor
    RuleResult copy(original);
    CHECK(copy.ruleName == "copy_test");
    CHECK(copy.ruleId == 999);
    CHECK(copy.success == true);
    CHECK(copy.skipped == false);
    
    // Test copy assignment
    RuleResult assigned;
    assigned = original;
    CHECK(assigned.ruleName == "copy_test");
    CHECK(assigned.ruleId == 999);
    CHECK(assigned.success == true);
    CHECK(assigned.skipped == false);
    
    // Test move constructor
    RuleResult moved(std::move(original));
    CHECK(moved.ruleName == "copy_test");
    CHECK(moved.ruleId == 999);
    CHECK(moved.success == true);
    CHECK(moved.skipped == false);
    
    // Test move assignment
    RuleResult moveAssigned;
    moveAssigned = std::move(copy);
    CHECK(moveAssigned.ruleName == "copy_test");
    CHECK(moveAssigned.ruleId == 999);
    CHECK(moveAssigned.success == true);
    CHECK(moveAssigned.skipped == false);
}

TEST_CASE("RuleResult edge cases") {
    RuleResult result;
    
    // Test with empty rule name
    result.ruleName = "";
    CHECK(result.ruleName.empty());
    
    // Test with very large rule ID
    result.ruleId = std::numeric_limits<int>::max();
    CHECK(result.ruleId == std::numeric_limits<int>::max());
    
    // Test with negative rule ID
    result.ruleId = -1;
    CHECK(result.ruleId == -1);
    
    // Test with very long rule name
    std::string longName(1000, 'a');
    result.ruleName = longName;
    CHECK(result.ruleName == longName);
    
    // Test with executedAt after completedAt (should still work)
    result.executedAt = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    result.completedAt = std::chrono::steady_clock::now();
    
    auto duration = result.duration();
    CHECK(duration.count() >= 0); // Should be non-negative
}