// db_example.cpp
// Demonstrates the fastrules-db extension: database persistence via SOCI.
// Supports PostgreSQL, MySQL, SQLite, and other backends.

#include <fastrules.hpp>
#include <fastrules/db_repository.hpp>
#include <iostream>

using namespace fastrules;
using namespace fastrules::ext;

int main() {
    try {
        // ================================================================
        // 1. Create a SOCI session (SQLite for this example)
        // ================================================================
        auto session = DbConnectionFactory::create("sqlite3", "rules.db");
        DbRuleRepository repo(session);

        // The schema is created automatically on first use
        std::cout << "Connected to database (SQLite: rules.db)\n";

        // ================================================================
        // 2. Create and save rules using builder
        // ================================================================
        auto rule1 = Rule::Builder("fraud-check")
            .withExpression("amount < 10000")
            .withAction("flagged = false")
            .withDescription("Fraud detection rule")
            .withPriority(100)
            .active(true)
            .build();

        auto rule2 = Rule::Builder("kyc-check")
            .withExpression("verified == true")
            .withAction("compliant = true")
            .withDescription("KYC verification rule")
            .withPriority(50)
            .active(true)
            .dependsOn("fraud-check")
            .build();

        repo.save(*rule1);
        repo.save(*rule2);
        std::cout << "Saved 2 rules to database\n";

        // ================================================================
        // 3. Query rules back
        // ================================================================
        auto loaded = repo.findById("fraud-check");
        if (loaded) {
            std::cout << "Loaded rule: " << loaded->id << "\n";
            std::cout << "  Expression: " << loaded->expression << "\n";
            std::cout << "  Priority: " << loaded->priority << "\n";
        }

        auto allRules = repo.findAll();
        std::cout << "Total rules in database: " << allRules.size() << "\n";

        for (const auto& rule : allRules) {
            std::cout << "  - " << rule.id << " (priority=" << rule.priority << ")\n";
        }

        // ================================================================
        // 4. Update an existing rule
        // ================================================================
        auto updated = Rule::Builder("fraud-check")
            .withExpression("amount < 5000")
            .withAction("flagged = true")
            .withPriority(200)
            .build();
        
        repo.save(*updated);
        std::cout << "\nUpdated fraud-check rule (new priority=200)\n";

        auto reloaded = repo.findById("fraud-check");
        if (reloaded) {
            std::cout << "Reloaded expression: " << reloaded->expression << "\n";
            std::cout << "Reloaded priority: " << reloaded->priority << "\n";
        }

        // ================================================================
        // 5. Execute workflow using database-backed rules
        // ================================================================
        LuaEngine engine;
        Workflow workflow;
        workflow.id = "compliance-check";

        for (const auto& rule : allRules) {
            workflow.rules.push_back(std::make_shared<Rule>(rule));
        }

        workflow.compile(engine);

        double amount = 3000;
        bool verified = true;
        std::vector<RuleParameter> params;
        params.emplace_back("amount", &amount);
        params.emplace_back("verified", &verified);

        auto results = workflow.execute(engine, params);
        for (const auto& result : results) {
            std::cout << "  Rule " << result.ruleId << ": " << (result.isSuccess() ? "PASS" : "FAIL") << "\n";
        }

        // ================================================================
        // 6. PostgreSQL example (commented — requires PG server)
        // ================================================================
        // auto pgSession = DbConnectionFactory::create(
        //     "postgresql",
        //     "dbname=rules host=localhost user=fastrules password=***
        // DbRuleRepository pgRepo(pgSession);
        // pgRepo.save(rule1);

        std::cout << "\nDB extension example complete.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
