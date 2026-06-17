#include <doctest/doctest.h>
#include <fastrules/xml_repository.hpp>
#include <filesystem>

using namespace fastrules;
using namespace fastrules::ext;

TEST_CASE("XmlRuleRepository basic CRUD") {
    auto tempFile = std::filesystem::temp_directory_path() / "test_rules.xml";
    std::filesystem::remove(tempFile);
    
    SUBCASE("Create and read rule") {
        XmlRuleRepository repo(tempFile);
        
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
    }
    
    SUBCASE("Update existing rule") {
        XmlRuleRepository repo(tempFile);
        
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
    
    SUBCASE("Delete rule") {
        XmlRuleRepository repo(tempFile);
        
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
    
    std::filesystem::remove(tempFile);
}
