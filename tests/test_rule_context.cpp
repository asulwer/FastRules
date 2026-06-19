#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <vector>
#include <map>
#include <future>

using namespace fastrules;

TEST_CASE("RuleContext basic functionality") {
    RuleContext ctx;
    
    // Test default state - we can't directly check count, but we can check if results exist
    auto nonExistent = ctx.getResult("test_rule");
    CHECK_FALSE(nonExistent.has_value());
    
    // Test storing and retrieving results
    RuleResult result;
    result.ruleName = "test_rule";
    result.ruleId = 1;
    result.success = true;
    
    ctx.setResult("test_rule", result);
    
    auto retrieved = ctx.getResult("test_rule");
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->ruleName == "test_rule");
    CHECK(retrieved->ruleId == 1);
    CHECK(retrieved->success);
    
    // Test non-existent result
    auto nonExistent2 = ctx.getResult("non_existent");
    CHECK_FALSE(nonExistent2.has_value());
}

TEST_CASE("RuleContext thread safety") {
    RuleContext ctx;
    
    // Test concurrent access
    std::vector<std::future<void>> futures;
    
    // Launch multiple threads to store results
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&ctx, i]() {
            RuleResult result;
            result.ruleName = "rule_" + std::to_string(i);
            result.ruleId = i;
            result.success = true;
            
            ctx.setResult("rule_" + std::to_string(i), result);
            
            // Also test retrieval
            auto retrieved = ctx.getResult("rule_" + std::to_string(i));
            if (retrieved.has_value()) {
                CHECK(retrieved->ruleId == i);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify each result can be retrieved
    for (int i = 0; i < 10; ++i) {
        auto result = ctx.getResult("rule_" + std::to_string(i));
        REQUIRE(result.has_value());
        CHECK(result->ruleId == i);
    }
}

TEST_CASE("RuleContext variable storage") {
    RuleContext ctx;
    
    // Test storing different types of variables
    ctx.setVariable("int_var", 42);
    ctx.setVariable("string_var", std::string("hello"));
    ctx.setVariable("double_var", 3.14);
    ctx.setVariable("bool_var", true);
    
    // Test retrieving variables
    auto int_var = ctx.getVariable("int_var");
    REQUIRE(int_var.has_value());
    CHECK(std::any_cast<int>(int_var.value()) == 42);
    
    auto string_var = ctx.getVariable("string_var");
    REQUIRE(string_var.has_value());
    CHECK(std::any_cast<std::string>(string_var.value()) == "hello");
    
    auto double_var = ctx.getVariable("double_var");
    REQUIRE(double_var.has_value());
    CHECK(std::any_cast<double>(double_var.value()) == doctest::Approx(3.14));
    
    auto bool_var = ctx.getVariable("bool_var");
    REQUIRE(bool_var.has_value());
    CHECK(std::any_cast<bool>(bool_var.value()) == true);
    
    // Test non-existent variable
    auto non_existent = ctx.getVariable("non_existent");
    CHECK_FALSE(non_existent.has_value());
}

TEST_CASE("RuleContext copy and assignment") {
    RuleContext ctx1;
    
    // Add some data
    RuleResult result;
    result.ruleName = "test_rule";
    result.ruleId = 1;
    result.success = true;
    ctx1.setResult("test_rule", result);
    
    ctx1.setVariable("test_var", 42);
    
    // Test copy constructor
    RuleContext ctx2(ctx1);
    auto result2 = ctx2.getResult("test_rule");
    CHECK(result2.has_value());
    CHECK(std::any_cast<int>(ctx2.getVariable("test_var").value()) == 42);
    
    // Test copy assignment
    RuleContext ctx3;
    ctx3 = ctx1;
    auto result3 = ctx3.getResult("test_rule");
    CHECK(result3.has_value());
    CHECK(std::any_cast<int>(ctx3.getVariable("test_var").value()) == 42);
    
    // Test move constructor
    RuleContext ctx4(std::move(ctx1));
    auto result4 = ctx4.getResult("test_rule");
    CHECK(result4.has_value());
    
    // Test move assignment
    RuleContext ctx5;
    ctx5 = std::move(ctx2);
    auto result5 = ctx5.getResult("test_rule");
    CHECK(result5.has_value());
}

TEST_CASE("RuleContext error handling") {
    RuleContext ctx;
    
    // Test setting and getting last error
    ctx.setLastError("test_rule", "Test error message");
    auto lastError = ctx.getLastError();
    REQUIRE(lastError.has_value());
    CHECK(lastError->second == "Test error message");
    
    // Test clearing error
    ctx.clearLastError();
    auto lastError2 = ctx.getLastError();
    CHECK_FALSE(lastError2.has_value());
}

TEST_CASE("RuleContext performance") {
    RuleContext ctx;
    
    // Test storing and retrieving many results
    auto start = std::chrono::high_resolution_clock::now();
    
    // Store many results
    for (int i = 0; i < 1000; ++i) {
        RuleResult result;
        result.ruleName = "rule_" + std::to_string(i);
        result.ruleId = i;
        result.success = true;
        ctx.setResult("rule_" + std::to_string(i), result);
    }
    
    // Retrieve many results
    for (int i = 0; i < 1000; ++i) {
        auto result = ctx.getResult("rule_" + std::to_string(i));
        REQUIRE(result.has_value());
        CHECK(result->ruleId == i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 2000 operations quickly
    CHECK(duration.count() < 1000); // 1 second should be more than enough
}

TEST_CASE("RuleContext child results") {
    RuleContext ctx;
    
    // Test storing results with child results
    RuleResult parentResult;
    parentResult.ruleName = "parent_rule";
    parentResult.ruleId = 1;
    parentResult.success = true;
    
    RuleResult childResult1;
    childResult1.ruleName = "child_rule_1";
    childResult1.ruleId = 2;
    childResult1.success = true;
    
    RuleResult childResult2;
    childResult2.ruleName = "child_rule_2";
    childResult2.ruleId = 3;
    childResult2.success = false;
    
    parentResult.childResults.push_back(childResult1);
    parentResult.childResults.push_back(childResult2);
    
    ctx.setResult("parent_rule", parentResult);
    
    // Retrieve and verify parent result with children
    auto retrieved = ctx.getResult("parent_rule");
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->ruleName == "parent_rule");
    CHECK(retrieved->childResults.size() == 2);
    CHECK(retrieved->childResults[0].ruleName == "child_rule_1");
    CHECK(retrieved->childResults[1].ruleName == "child_rule_2");
    CHECK(retrieved->childResults[0].success == true);
    CHECK(retrieved->childResults[1].success == false);
}

TEST_CASE("RuleContext complex variables") {
    RuleContext ctx;
    
    // Test storing complex variable types
    std::vector<int> intVector = {1, 2, 3, 4, 5};
    ctx.setVariable("int_vector", intVector);
    
    std::map<std::string, int> stringIntMap = {{"a", 1}, {"b", 2}, {"c", 3}};
    ctx.setVariable("string_int_map", stringIntMap);
    
    // Verify variables were stored
    auto retrievedVector = ctx.getVariable("int_vector");
    REQUIRE(retrievedVector.has_value());
    CHECK(std::any_cast<std::vector<int>>(retrievedVector.value()) == intVector);
}

TEST_CASE("RuleContext cleanup") {
    RuleContext ctx;
    
    // Test that context can be properly cleaned up
    for (int i = 0; i < 100; ++i) {
        RuleResult result;
        result.ruleName = "cleanup_test_" + std::to_string(i);
        result.ruleId = i;
        result.success = true;
        ctx.setResult("cleanup_test_" + std::to_string(i), result);
    }
    
    // Test that context can be reused by clearing it
    ctx.clear();
    
    // Add a new result
    RuleResult result;
    result.ruleName = "new_result";
    result.ruleId = 999;
    result.success = true;
    ctx.setResult("new_result", result);
    
    auto retrieved = ctx.getResult("new_result");
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->ruleId == 999);
}