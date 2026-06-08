#include <catch2/catch_test_macros.hpp>
#include <fastrules/db_repository.hpp>
#include <filesystem>

using namespace fastrules;
using namespace fastrules::ext;

TEST_CASE("DbRuleRepository with SQLite", "[db]") {
    auto tempFile = std::filesystem::temp_directory_path() / "test_rules.db";
    std::filesystem::remove(tempFile);
    
    {
        auto session = DbConnectionFactory::create("sqlite3", tempFile.string());
        DbRuleRepository repo(session);
        
        SECTION("Create and read rule") {
        auto rule = Rule::Builder("test-1")
            .withExpression("age >= 18")
            .withAction("eligible = true")
            .withPriority(10)
            .active(true)
            .build();
        
        repo.save(*rule);
        
        auto found = repo.findById("test-1");
        REQUIRE(found.has_value());
        REQUIRE(found->id == "test-1");
        REQUIRE(found->expression == "age >= 18");
        REQUIRE(found->isActive == true);
        REQUIRE(found->priority == 10);
    }
    
    SECTION("Update existing rule") {
        auto rule = Rule::Builder("test-1")
            .withExpression("age >= 18")
            .build();
        repo.save(*rule);
        
        auto updated = Rule::Builder("test-1")
            .withExpression("age >= 21")
            .withAction("eligible = false")
            .withPriority(20)
            .build();
        repo.save(*updated);
        
        auto found = repo.findById("test-1");
        REQUIRE(found->expression == "age >= 21");
        REQUIRE(found->priority == 20);
    }
    
    SECTION("Delete rule") {
        auto rule = Rule::Builder("test-1")
            .withExpression("x > 0")
            .build();
        repo.save(*rule);
        
        repo.remove("test-1");
        
        REQUIRE_FALSE(repo.exists("test-1"));
        REQUIRE(repo.count() == 0);
    }
    
    SECTION("Find all rules") {
        auto rule1 = Rule::Builder("rule-1")
            .withExpression("x > 0")
            .build();
        auto rule2 = Rule::Builder("rule-2")
            .withExpression("y > 0")
            .build();
        
        repo.save(*rule1);
        repo.save(*rule2);
        
        auto all = repo.findAll();
        REQUIRE(all.size() == 2);
    }
    
    SECTION("Complex rule with parameters and dependencies") {
        auto rule = Rule::Builder("complex-1")
            .withExpression("age >= 18")
            .withAction("eligible = true")
            .withParameterNames({"age", "name"})
            .dependsOn("base-rule")
            .build();
        
        repo.save(*rule);
        
        auto found = repo.findById("complex-1");
        REQUIRE(found->parameterNames.size() == 2);
        REQUIRE(found->parameterNames[0] == "age");
        REQUIRE(found->dependsOnRuleId.has_value());
        REQUIRE(found->dependsOnRuleId.value() == "base-rule");
    }
    }
    
    std::filesystem::remove(tempFile);
}

TEST_CASE("DbWorkflowRepository with SQLite", "[db]") {
    auto tempFile = std::filesystem::temp_directory_path() / "test_workflows.db";
    std::filesystem::remove(tempFile);
    
    {
        auto session = DbConnectionFactory::create("sqlite3", tempFile.string());
        DbRuleRepository ruleRepo(session);
        DbWorkflowRepository repo(session);
        
        SECTION("Save and load workflow") {
            Workflow wf;
            wf.id = "wf-1";
            wf.description = "Test workflow";
            wf.isActive = true;
            
            repo.save(wf);
            
            auto found = repo.findById("wf-1");
            REQUIRE(found.has_value());
            REQUIRE(found->id == "wf-1");
            REQUIRE(found->description == "Test workflow");
        }
    }
    
    std::filesystem::remove(tempFile);
}

