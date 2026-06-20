// db_example.cpp
// Demonstrates the fastrules-db extension: database persistence via SOCI.
// Supports PostgreSQL, MySQL, SQLite, and other backends.

#include <fastrules.hpp>
#include <fastrules/db_repository.hpp>
#include <iostream>
#include <memory>
#include <filesystem>

using namespace fastrules;
using namespace fastrules::ext;

// Helper to create a rule (Rule is move-only)
std::unique_ptr<Rule> createRule(int id, const std::string& name, const std::string& expr) {
    auto rule = std::make_unique<Rule>();
    rule->id = id;
    rule->name = name;
    rule->expression = expr;
    rule->isActive = true;
    return rule;
}

int main() {
    try {
        // ================================================================
        // 1. Create a SOCI session (SQLite for this example)
        // ================================================================
        auto session = DbConnectionFactory::create("sqlite3", (std::filesystem::current_path() / "rules.db").string());
        DbRuleRepository repo(session);

        // The schema is created automatically on first use
        std::cout << "Connected to database (SQLite: rules.db)\n";

        // ================================================================
        // 2. Create and save rules
        // ================================================================
        {
            auto rule1 = createRule(1, "fraud-check", "amount < 10000");
            rule1->action = "flagged = false";
            rule1->description = "Fraud detection rule";
            rule1->priority = 100;
            repo.save(*rule1);
        }
        
        {
            auto rule2 = createRule(2, "kyc-check", "verified == true");
            rule2->action = "compliant = true";
            rule2->description = "KYC verification rule";
            rule2->priority = 50;
            rule2->dependsOnRuleName = "fraud-check";
            repo.save(*rule2);
        }
        
        std::cout << "Saved 2 rules to database\n";

        // ================================================================
        // 3. Query rules back
        // ================================================================
        auto loaded = repo.findById(1);
        if (loaded) {
            std::cout << "Loaded rule: " << loaded->id << " (" << loaded->name << ")\n";
            std::cout << "  Expression: " << loaded->expression << "\n";
            std::cout << "  Priority: " << loaded->priority << "\n";
        }

        auto allRules = repo.findAll();
        std::cout << "Total rules in database: " << allRules.size() << "\n";

        for (const auto& rule : allRules) {
            std::cout << "  - " << rule.id << ": " << rule.name << " (priority=" << rule.priority << ")\n";
        }

        // ================================================================
        // 4. Update an existing rule
        // ================================================================
        {
            auto updated = createRule(1, "fraud-check", "amount < 5000");
            updated->action = "flagged = true";
            updated->priority = 200;
            repo.save(*updated);
        }
        
        std::cout << "\nUpdated fraud-check rule (new priority=200)\n";

        auto reloaded = repo.findById(1);
        if (reloaded) {
            std::cout << "Reloaded expression: " << reloaded->expression << "\n";
            std::cout << "Reloaded priority: " << reloaded->priority << "\n";
        }

        // ================================================================
        // 5. Execute workflow using database-backed rules
        // ================================================================
        LuaEngine engine;
        Workflow workflow;
        workflow.id = 1;
        workflow.description = "compliance-check";

        for (const auto& rule : allRules) {
            // Rule is move-only, so we need to create a new one for the workflow
            auto workflowRule = std::make_unique<Rule>();
            workflowRule->id = rule.id;
            workflowRule->name = rule.name;
            workflowRule->expression = rule.expression;
            workflowRule->action = rule.action;
            workflowRule->description = rule.description;
            workflowRule->priority = rule.priority;
            workflowRule->isActive = rule.isActive;
            workflow.rules.push_back(std::move(workflowRule));
        }

        workflow.compile(engine);

        // Execute with test parameters
        std::vector<RuleParameter> params;
        params.emplace_back("amount", 3000.0);
        params.emplace_back("verified", true);

        auto results = workflow.execute(engine, params);
        std::cout << "\nExecution results:\n";
        for (const auto& result : results) {
            std::cout << "Rule " << result.ruleName << ": " << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        // ================================================================
        // 6. PostgreSQL example (commented - requires PG server)
        // ================================================================
        // auto pgSession = DbConnectionFactory::create(
        //     "postgresql",
        //     "dbname=rules host=localhost user=fastrules password=***");
        // DbRuleRepository pgRepo(pgSession);
        // auto rule = createRule(1, "test", "true");
        // pgRepo.save(*rule);

        std::cout << "\nDB extension example complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}