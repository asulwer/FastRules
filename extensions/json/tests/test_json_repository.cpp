#include <catch2/catch_test_macros.hpp>
#include <fastrules/json_repository.hpp>
#include <filesystem>
#include <fstream>

using namespace fastrules;
using namespace fastrules::ext;

TEST_CASE("JsonRuleRepository basic CRUD", "[json]") {
    auto tempFile = std::filesystem::temp_directory_path() / "test_rules.json";
    
    // Clean up before test
    std::filesystem::remove(tempFile);
    
    SECTION("Create and read rule") {
        JsonRuleRepository repo(tempFile);
        
        Rule rule;
        rule.id = 1;
        rule.expression = "age >= 18";
        rule.action = "eligible = true";
        rule.isActive = true;
        rule.priority = 10;
        
        repo.save(rule);
        repo.flush();
        
        auto found = repo.findById(1);
        REQUIRE(found.has_value());
        REQUIRE(found->id == 1);
        REQUIRE(found->expression == "age >= 18");
        REQUIRE(found->isActive == true);
        REQUIRE(found->priority == 10);
    }
    
    SECTION("Update existing rule") {
        JsonRuleRepository repo(tempFile);
        
        Rule rule;
        rule.id = 1;
        rule.expression = "age >= 18";
        rule.action = "eligible = true";
        repo.save(rule);
        repo.flush();
        
        Rule updated;
        updated.id = 1;
        updated.expression = "age >= 21";
        updated.action = "eligible = false";
        updated.priority = 20;
        repo.save(updated);
        repo.flush();
        
        auto found = repo.findById(1);
        REQUIRE(found->expression == "age >= 21");
        REQUIRE(found->priority == 20);
    }
    
    SECTION("Delete rule") {
        JsonRuleRepository repo(tempFile);
        
        Rule rule;
        rule.id = 1;
        rule.expression = "x > 0";
        rule.action = "a = 1";
        repo.save(rule);
        repo.flush();
        
        repo.remove(1);
        repo.flush();
        
        REQUIRE_FALSE(repo.exists(1));
        REQUIRE(repo.count() == 0);
    }
    
    SECTION("Find all rules") {
        JsonRuleRepository repo(tempFile);
        
        Rule r1;
        r1.id = 1;
        r1.expression = "x > 0";
        r1.action = "a = 1";
        
        Rule r2;
        r2.id = 2;
        r2.expression = "y > 0";
        r2.action = "b = 2";
        
        repo.save(r1);
        repo.save(r2);
        repo.flush();
        
        auto all = repo.findAll();
        REQUIRE(all.size() == 2);
    }
    
    // Clean up after test
    std::filesystem::remove(tempFile);
}
