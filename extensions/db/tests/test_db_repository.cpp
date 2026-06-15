#include <catch2/catch_test_macros.hpp>
#include <fastrules/db_repository.hpp>
#include <filesystem>

using namespace fastrules;
using namespace fastrules::ext;

// Helper to create a rule
std::unique_ptr<Rule> createRule(int id, const std::string& name, const std::string& expr) {
    auto rule = std::make_unique<Rule>();
    rule->id = id;
    rule->name = name;
    rule->expression = expr;
    rule->isActive = true;
    return rule;
}

TEST_CASE("DbRuleRepository with SQLite", "[db]") {
    auto tempFile = std::filesystem::temp_directory_path() / "test_rules.db";
    std::filesystem::remove(tempFile);
    
    {
        auto session = DbConnectionFactory::create("sqlite3", tempFile.string());
        DbRuleRepository repo(session);
        
        SECTION("Create and read rule") {
            auto rule = createRule(1, "test-1", "age >= 18");
            rule->action = "eligible = true";
            rule->priority = 10;
            
            repo.save(*rule);
            
            auto found = repo.findById(1);
            REQUIRE(found.has_value());
            REQUIRE(found->id == 1);
            REQUIRE(found->expression == "age >= 18");
            REQUIRE(found->isActive == true);
            REQUIRE(found->priority == 10);
        }
        
        SECTION("Update existing rule") {
            auto rule = createRule(1, "test-1", "age >= 18");
            repo.save(*rule);
            
            auto updated = createRule(1, "test-1", "age >= 21");
            updated->action = "eligible = false";
            updated->priority = 20;
            repo.save(*updated);
            
            auto found = repo.findById(1);
            REQUIRE(found->expression == "age >= 21");
            REQUIRE(found->priority == 20);
        }
        
        SECTION("Delete rule") {
            auto rule = createRule(1, "test-1", "x > 0");
            repo.save(*rule);
            
            repo.remove(1);
            
            REQUIRE_FALSE(repo.exists(1));
            REQUIRE(repo.count() == 0);
        }
        
        SECTION("Find all rules") {
            auto rule1 = createRule(1, "rule-1", "x > 0");
            auto rule2 = createRule(2, "rule-2", "y > 0");
            
            repo.save(*rule1);
            repo.save(*rule2);
            
            auto all = repo.findAll();
            REQUIRE(all.size() == 2);
        }
        
        SECTION("Complex rule with parameters and dependencies") {
            auto rule = createRule(1, "complex-1", "age >= 18");
            rule->action = "eligible = true";
            rule->description = "Complex test rule";
            rule->priority = 100;
            rule->timeout = std::chrono::milliseconds(5000);
            rule->dependsOnRuleName = "parent-rule";
            
            repo.save(*rule);
            
            auto found = repo.findById(1);
            REQUIRE(found->expression == "age >= 18");
            REQUIRE(found->action == "eligible = true");
            REQUIRE(found->priority == 100);
            REQUIRE(found->timeout == std::chrono::milliseconds(5000));
            REQUIRE(found->dependsOnRuleName == "parent-rule");
        }
    }
    
    std::filesystem::remove(tempFile);
}