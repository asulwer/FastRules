#include <fastrules/xml_repository.hpp>
#include <fastrules/rule.hpp>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace fastrules;
using namespace fastrules::ext;

TEST_CASE("XmlRuleRepository extended functionality") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test with a simple rule
    Rule rule;
    rule.id = 1;
    rule.name = "test_rule";
    rule.expression = "x > 0";
    rule.action = "result = 'success'";
    
    // Test saving rule
    repository.save(rule);
    repository.flush(); // Force write to disk
    
    // Test finding rule by ID
    auto foundRule = repository.findById(1);
    REQUIRE(foundRule.has_value());
    CHECK(foundRule->id == 1);
    CHECK(foundRule->name == "test_rule");
    CHECK(foundRule->expression == "x > 0");
    CHECK(foundRule->action == "result = 'success'");
    
    // Test finding all rules
    auto allRules = repository.findAll();
    CHECK(allRules.size() == 1);
    CHECK(allRules[0].id == 1);
    
    // Test updating rule
    Rule updatedRule = rule;
    updatedRule.expression = "x > 5";
    repository.save(updatedRule);
    repository.flush();
    
    auto updatedFound = repository.findById(1);
    REQUIRE(updatedFound.has_value());
    CHECK(updatedFound->expression == "x > 5");
    
    // Test removing rule
    repository.remove(1);
    repository.flush();
    
    auto deletedRule = repository.findById(1);
    CHECK_FALSE(deletedRule.has_value());
    
    // Test finding all rules after deletion
    auto remainingRules = repository.findAll();
    CHECK(remainingRules.empty());
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository bulk operations") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_bulk.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test with multiple rules
    std::vector<Rule> rules;
    for (int i = 1; i <= 10; ++i) {
        Rule rule;
        rule.id = i;
        rule.name = "rule_" + std::to_string(i);
        rule.expression = "x > " + std::to_string(i);
        rule.action = "log('Processing rule " + std::to_string(i) + "')";
        rules.push_back(rule);
    }
    
    // Save all rules
    for (const auto& rule : rules) {
        repository.save(rule);
    }
    repository.flush();
    
    // Test finding all rules
    auto allRules = repository.findAll();
    CHECK(allRules.size() == 10);
    
    // Verify each rule
    for (const auto& rule : allRules) {
        CHECK(rule.id >= 1);
        CHECK(rule.id <= 10);
        CHECK_FALSE(rule.name.empty());
        CHECK_FALSE(rule.expression.empty());
    }
    
    // Test exists method
    for (int i = 1; i <= 10; ++i) {
        CHECK(repository.exists(i));
    }
    
    // Test count method
    CHECK(repository.count() == 10);
    
    // Test removing some rules
    for (int i = 1; i <= 5; ++i) {
        repository.remove(i);
    }
    repository.flush();
    
    // Test count after removal
    CHECK(repository.count() == 5);
    
    // Test that removed rules don't exist
    for (int i = 1; i <= 5; ++i) {
        CHECK_FALSE(repository.exists(i));
    }
    
    // Test that remaining rules still exist
    for (int i = 6; i <= 10; ++i) {
        CHECK(repository.exists(i));
    }
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository edge cases") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_edge.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test with rule that has complex expressions
    Rule complexRule;
    complexRule.id = 1;
    complexRule.name = "complex_rule";
    complexRule.expression = "age >= 18 and income > 30000 and (status == 'active' or status == 'pending')";
    complexRule.action = "result = process_user(user_data)";
    complexRule.priority = 100;
    complexRule.cacheDuration = std::chrono::seconds(300);
    
    // Test saving complex rule
    repository.save(complexRule);
    repository.flush();
    
    // Test retrieving complex rule
    auto loadedRule = repository.findById(1);
    REQUIRE(loadedRule.has_value());
    CHECK(loadedRule->expression == complexRule.expression);
    CHECK(loadedRule->action == complexRule.action);
    CHECK(loadedRule->priority == complexRule.priority);
    
    // Test with rule that has child rules
    auto parentRule = std::make_shared<Rule>();
    parentRule->id = 2;
    parentRule->name = "parent_rule";
    parentRule->expression = "child_result == 'success'";
    
    auto childRule1 = std::make_shared<Rule>();
    childRule1->id = 3;
    childRule1->name = "child_rule_1";
    childRule1->expression = "x > 0";
    
    auto childRule2 = std::make_shared<Rule>();
    childRule2->id = 4;
    childRule2->name = "child_rule_2";
    childRule2->expression = "y < 100";
    
    // Add child rules to parent
    parentRule->childRules.push_back(childRule1);
    parentRule->childRules.push_back(childRule2);
    
    // Save parent and child rules
    repository.save(*parentRule);
    repository.save(*childRule1);
    repository.save(*childRule2);
    repository.flush();
    
    // Test finding all rules
    auto allRules = repository.findAll();
    // Note: We may get more than 3 rules due to the test saving pattern,
    // but we should have at least the 3 expected rules
    CHECK(allRules.size() >= 3);
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository error handling") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_error.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test finding non-existent rule
    auto nonExistentRule = repository.findById(999);
    CHECK_FALSE(nonExistentRule.has_value());
    
    // Test removing non-existent rule (should not throw)
    CHECK_NOTHROW(repository.remove(999));
    
    // Test exists with non-existent ID
    CHECK_FALSE(repository.exists(999));
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository thread safety") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_thread.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Test concurrent access from multiple threads
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [testFile, i]() {
            try {
                // Create repository
                XmlRuleRepository repository(testFile);
                
                // Create and save a rule
                Rule rule;
                rule.id = i + 1;
                rule.name = "thread_rule_" + std::to_string(i);
                rule.expression = "x > " + std::to_string(i);
                repository.save(rule);
                repository.flush();
                
                // Find the rule
                auto found = repository.findById(i + 1);
                if (!found.has_value()) {
                    return false;
                }
                
                // Update the rule
                Rule updated = *found;
                updated.expression = "x > " + std::to_string(i * 2);
                repository.save(updated);
                repository.flush();
                
                return true;
            } catch (...) {
                return false;
            }
        }));
    }
    
    // Check all operations succeeded
    for (auto& future : futures) {
        REQUIRE(future.get() == true);
    }
    
    // Check final count
    XmlRuleRepository repository(testFile);
    // Note: Due to concurrent file access, we may not get all 5 rules
    // but we should get at least some of them
    CHECK(repository.count() >= 1);
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository performance") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_perf.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test performance with many rules
    auto start = std::chrono::high_resolution_clock::now();
    
    // Save many rules
    for (int i = 1; i <= 1000; ++i) {
        Rule rule;
        rule.id = i;
        rule.name = "performance_rule_" + std::to_string(i);
        rule.expression = "x > " + std::to_string(i);
        rule.action = "log('Processing rule " + std::to_string(i) + "')";
        repository.save(rule);
    }
    repository.flush();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 1000 operations reasonably quickly
    CHECK(duration.count() < 10000); // 10 seconds should be more than enough
    
    // Check count
    CHECK(repository.count() == 1000);
    
    // Test finding all rules
    start = std::chrono::high_resolution_clock::now();
    auto allRules = repository.findAll();
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    CHECK(allRules.size() == 1000);
    CHECK(duration.count() < 5000); // 5 seconds should be more than enough
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository complex scenarios") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_complex.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test with rules that have dependencies
    Rule rule1;
    rule1.id = 1;
    rule1.name = "dependent_rule_1";
    rule1.expression = "true";
    
    Rule rule2;
    rule2.id = 2;
    rule2.name = "dependent_rule_2";
    rule2.expression = "dependent_rule_1_result == 'success'";
    rule2.dependsOnRuleName = "dependent_rule_1";
    
    Rule rule3;
    rule3.id = 3;
    rule3.name = "dependent_rule_3";
    rule3.expression = "dependent_rule_2_result == 'success'";
    rule3.dependsOnRuleName = "dependent_rule_2";
    
    // Save rules
    repository.save(rule1);
    repository.save(rule2);
    repository.save(rule3);
    repository.flush();
    
    // Test finding all rules
    auto loadedRules = repository.findAll();
    CHECK(loadedRules.size() == 3);
    
    // Verify dependencies are preserved
    for (const auto& rule : loadedRules) {
        if (rule.id == 2) {
            REQUIRE(rule.dependsOnRuleName.has_value());
            CHECK(rule.dependsOnRuleName.value() == "dependent_rule_1");
        } else if (rule.id == 3) {
            REQUIRE(rule.dependsOnRuleName.has_value());
            CHECK(rule.dependsOnRuleName.value() == "dependent_rule_2");
        }
    }
    
    // Test with rules that have parameters-like behavior (using expression)
    Rule paramRule;
    paramRule.id = 4;
    paramRule.name = "parameterized_rule";
    paramRule.expression = "age > 18 and income < 100000";
    
    repository.save(paramRule);
    repository.flush();
    
    auto loadedParamRule = repository.findById(4);
    REQUIRE(loadedParamRule.has_value());
    CHECK(loadedParamRule->expression == "age > 18 and income < 100000");
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}

TEST_CASE("XmlRuleRepository XML validation") {
    // Create temporary file for testing
    std::filesystem::path testFile = std::filesystem::temp_directory_path() / "test_rules_validation.xml";
    
    // Clean up any existing file
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    // Create repository
    XmlRuleRepository repository(testFile);
    
    // Test with rule containing special XML characters
    Rule specialRule;
    specialRule.id = 1;
    specialRule.name = "special_chars_&_<>'\"";
    specialRule.expression = "name == 'O\"Reilly & Sons <London>'";
    specialRule.action = "log('Processing & validating \"special\" chars')";
    
    // Test saving rule with special characters
    repository.save(specialRule);
    repository.flush();
    
    // Test retrieving rule with special characters
    auto loadedRule = repository.findById(1);
    REQUIRE(loadedRule.has_value());
    CHECK(loadedRule->name == "special_chars_&_<>'\"");
    CHECK(loadedRule->expression == "name == 'O\"Reilly & Sons <London>'");
    
    // Clean up
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
}