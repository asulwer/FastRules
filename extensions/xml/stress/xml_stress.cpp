// xml_stress.cpp
// Stress scenarios for the XML extension.

#include "stress_runner.hpp"
#include "stress_helpers.hpp"
#include <fastrules/xml_loader.hpp>
#include <fastrules/xml_repository.hpp>
#include <fastrules/lua_engine.hpp>
#include <fastrules/workflow.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <cstring>

using namespace fastrules;
using namespace fastrules::stress;
using namespace fastrules::ext;

namespace {

std::filesystem::path tempRepoPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("fastrules-stress-xml-" + name + ".xml");
}

static StressResult xmlLoadExecuteRoundtrip(const StressConfig& cfg) {
    auto engine = std::make_shared<LuaEngine>();
    engine->setLogger(nullptr);
    auto wf = makeWorkflow(1, cfg.rules, cfg.parameters);
    wf.compile(*engine);
    auto params = makeParameters(static_cast<int>(cfg.parameters), 0);

    std::string xml;
    StressRunner runner;
    return runner.run("xml load/execute roundtrip", cfg, [&](size_t i) {
        xml = XmlLoader::saveWorkflow(wf);
        auto loaded = XmlLoader::loadWorkflow(xml);
        loaded.compile(*engine);
        (void)loaded.execute(*engine, params);
        (void)i;
    });
}

static StressResult xmlRuleRepositoryChurn(const StressConfig& cfg) {
    auto path = tempRepoPath("rules");
    std::filesystem::remove(path);
    XmlRuleRepository repo(path);

    for (size_t i = 0; i < cfg.rules; ++i) {
        Rule r = *makeRule(static_cast<int>(i + 1), static_cast<int>(cfg.parameters));
        repo.save(r);
    }
    repo.flush();

    StressRunner runner;
    return runner.run("xml rule repository churn", cfg, [&](size_t i) {
        size_t idx = i % cfg.rules;
        auto found = repo.findById(static_cast<int>(idx + 1));
        if (found) {
            repo.save(*found);
        }
    });
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::function<StressResult(const StressConfig&)>> scenarios = {
        xmlLoadExecuteRoundtrip,
        xmlRuleRepositoryChurn,
    };
    return runSuite("xml", scenarios, argc, argv);
}
