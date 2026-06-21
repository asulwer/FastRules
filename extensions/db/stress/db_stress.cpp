// db_stress.cpp
// Stress scenarios for the DB extension using an in-memory SQLite database.

#include "stress_runner.hpp"
#include "stress_helpers.hpp"
#include <fastrules/db_repository.hpp>
#include <fastrules/lua_engine.hpp>
#include <fastrules/workflow.hpp>
#include <iostream>
#include <memory>
#include <sstream>

using namespace fastrules;
using namespace fastrules::stress;
using namespace fastrules::ext;

namespace {

std::shared_ptr<soci::session> freshSession() {
    // In-memory database: each call returns a clean database.
    return DbConnectionFactory::create("sqlite3", ":memory:");
}

static StressResult dbRuleChurn(const StressConfig& cfg) {
    auto session = freshSession();
    DbRuleRepository repo(session);
    repo.createSchema();

    StressRunner runner;
    return runner.run("db rule churn", cfg, [&](size_t i) {
        int id = static_cast<int>((i % cfg.rules) + 1);
        Rule r = *makeRule(id, static_cast<int>(cfg.parameters));
        try {
            repo.save(r);
            auto found = repo.findById(id);
            (void)found;
        } catch (const std::exception& e) {
            std::cerr << "db rule churn error: " << e.what() << "\n";
            throw;
        }
    });
}

static StressResult dbWorkflowChurn(const StressConfig& cfg) {
    auto session = freshSession();
    // Workflow repo queries the rules table, so ensure it exists first.
    DbRuleRepository ruleRepo(session);
    DbWorkflowRepository repo(session);
    repo.createSchema();

    StressRunner runner;
    return runner.run("db workflow churn", cfg, [&](size_t i) {
        int id = static_cast<int>((i % cfg.rules) + 1);
        auto wf = makeWorkflow(id, 5, cfg.parameters);
        try {
            repo.save(wf);
            auto found = repo.findById(id);
            (void)found;
        } catch (const std::exception& e) {
            std::cerr << "db workflow churn error: " << e.what() << "\n";
            throw;
        }
    });
}

static StressResult dbCompileExecuteFromRepo(const StressConfig& cfg) {
    auto session = freshSession();
    DbRuleRepository ruleRepo(session);
    DbWorkflowRepository wfRepo(session);
    ruleRepo.createSchema();
    wfRepo.createSchema();

    // Seed rules into the database.
    for (size_t i = 0; i < cfg.rules; ++i) {
        Rule r = *makeRule(static_cast<int>(i + 1), static_cast<int>(cfg.parameters));
        ruleRepo.save(r);
    }

    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    StressRunner runner;
    return runner.run("db compile+execute from repo", cfg, [&](size_t) {
        auto all = ruleRepo.findAll();
        if (all.empty()) return;

        Workflow wf;
        wf.id = 1;
        for (const auto& r : all) {
            auto wr = std::make_unique<Rule>();
            *wr = r; // Rule is copyable?
            wf.rules.push_back(std::move(wr));
        }
        wf.compile(*engine);
        (void)wf.execute(*engine, params);
    });
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::function<StressResult(const StressConfig&)>> scenarios = {
        dbRuleChurn,
        dbWorkflowChurn,
        dbCompileExecuteFromRepo,
    };
    return runSuite("db", scenarios, argc, argv);
}
