#include <fastrules/db_repository.hpp>
#include <fastrules/rule.hpp>
#include <fastrules/workflow.hpp>
#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>
#include <doctest/doctest.h>
#include <memory>
#include <vector>
#include <chrono>

using namespace fastrules;
using namespace fastrules::ext;

TEST_CASE("DbRuleRepository extended functionality") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
    // Test with a simple rule
    Rule rule;
    rule.id = 1;
    rule.name = "test_rule";
    rule.expression = "x > 0";
    rule.action = "result = 'success'";
    
    // Test saving rule
    repository.save(rule);
    
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
    
    auto updatedFound = repository.findById(1);
    REQUIRE(updatedFound.has_value());
    CHECK(updatedFound->expression == "x > 5");
    
    // Test removing rule
    repository.remove(1);
    
    auto deletedRule = repository.findById(1);
    CHECK_FALSE(deletedRule.has_value());
    
    // Test finding all rules after deletion
    auto remainingRules = repository.findAll();
    CHECK(remainingRules.empty());
}

TEST_CASE("DbRuleRepository bulk operations") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
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
}

TEST_CASE("DbRuleRepository edge cases") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
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
    
    // Test finding all rules
    auto allRules = repository.findAll();
    CHECK(allRules.size() == 3);
    
    // Test clearing all rules
    repository.clear();
    CHECK(repository.count() == 0);
}

TEST_CASE("DbRuleRepository error handling") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
    // Test finding non-existent rule
    auto nonExistentRule = repository.findById(999);
    CHECK_FALSE(nonExistentRule.has_value());
    
    // Test removing non-existent rule (should not throw)
    CHECK_NOTHROW(repository.remove(999));
    
    // Test exists with non-existent ID
    CHECK_FALSE(repository.exists(999));
}

TEST_CASE("DbWorkflowRepository extended functionality") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repositories
    DbRuleRepository ruleRepository(session);
    DbWorkflowRepository workflowRepository(session);
    
    // Create schemas
    ruleRepository.createSchema();
    workflowRepository.createSchema();
    
    // Create some rules
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->name = "rule_1";
    rule1->expression = "x > 0";
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->name = "rule_2";
    rule2->expression = "y < 100";
    
    // Save rules to rule repository first
    ruleRepository.save(*rule1);
    ruleRepository.save(*rule2);
    
    // Create workflow using rule IDs
    Workflow workflow;
    workflow.id = 1;
    workflow.name = "test_workflow";
    
    // Test saving workflow
    workflowRepository.save(workflow);
    
    // Test finding workflow by ID
    auto loadedWorkflow = workflowRepository.findById(1);
    REQUIRE(loadedWorkflow.has_value());
    CHECK(loadedWorkflow->id == 1);
    CHECK(loadedWorkflow->name == "test_workflow");
    
    // Test finding all workflows
    auto allWorkflows = workflowRepository.findAll();
    CHECK(allWorkflows.size() == 1);
    CHECK(allWorkflows[0].id == 1);
    
    // Test updating workflow
    loadedWorkflow->name = "updated_workflow";
    workflowRepository.save(*loadedWorkflow);
    
    auto updatedFound = workflowRepository.findById(1);
    REQUIRE(updatedFound.has_value());
    CHECK(updatedFound->name == "updated_workflow");
    
    // Test removing workflow
    workflowRepository.remove(1);
    
    auto deletedWorkflow = workflowRepository.findById(1);
    CHECK_FALSE(deletedWorkflow.has_value());
}

TEST_CASE("DbRuleRepository thread safety") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
    // Test concurrent access from multiple threads
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [&repository, i]() {
            try {
                // Create and save a rule
                Rule rule;
                rule.id = i + 1;
                rule.name = "thread_rule_" + std::to_string(i);
                rule.expression = "x > " + std::to_string(i);
                repository.save(rule);
                
                // Find the rule
                auto found = repository.findById(i + 1);
                if (!found.has_value()) {
                    return false;
                }
                
                // Update the rule
                Rule updated = *found;
                updated.expression = "x > " + std::to_string(i * 2);
                repository.save(updated);
                
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
    CHECK(repository.count() == 5);
}

TEST_CASE("DbRuleRepository performance") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
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
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 1000 operations quickly
    CHECK(duration.count() < 5000); // 5 seconds should be more than enough
    
    // Check count
    CHECK(repository.count() == 1000);
    
    // Test finding all rules
    start = std::chrono::high_resolution_clock::now();
    auto allRules = repository.findAll();
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    CHECK(allRules.size() == 1000);
    CHECK(duration.count() < 5000); // 5 seconds should be more than enough
}

TEST_CASE("DbRuleRepository complex scenarios") {
    // Create in-memory SQLite database for testing
    auto session = std::make_shared<soci::session>(soci::sqlite3, ":memory:");
    
    // Create repository
    DbRuleRepository repository(session);
    
    // Create schema
    repository.createSchema();
    
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
    
    auto loadedParamRule = repository.findById(4);
    REQUIRE(loadedParamRule.has_value());
    CHECK(loadedParamRule->expression == "age > 18 and income < 100000");
}