#include <fastrules/json_loader.hpp>
#include <fastrules/rule.hpp>
#include <fastrules/workflow.hpp>

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <optional>
#include <chrono>
#include <atomic>

namespace fastrules {

namespace {
    // Thread-safe ID generation
    std::atomic<int> ruleIdCounter{0};
}

std::shared_ptr<Rule> JsonLoader::parseRule(const nlohmann::json& j) {
    auto rule = std::make_shared<Rule>();

    if (j.contains("id")) {
        if (j["id"].is_number_integer()) {
            rule->id = j["id"];
        } else if (j["id"].is_string()) {
            rule->id = std::stoi(j["id"].get<std::string>());
        } else {
            rule->id = ++ruleIdCounter;
        }
    } else {
        rule->id = ++ruleIdCounter;
    }

    if (j.contains("description") && j["description"].is_string()) {
        rule->description = j["description"];
    }

    if (j.contains("isActive") && j["isActive"].is_boolean()) {
        rule->isActive = j["isActive"];
    }

    if (j.contains("priority") && j["priority"].is_number_integer()) {
        rule->priority = j["priority"];
    }

    if (j.contains("expression") && j["expression"].is_string()) {
        rule->expression = j["expression"];
    }

    if (j.contains("action") && j["action"].is_string()) {
        rule->action = j["action"];
    }

    if (j.contains("dependsOnRuleId") && !j["dependsOnRuleId"].is_null()) {
        if (j["dependsOnRuleId"].is_number_integer()) {
            rule->dependsOnRuleId = j["dependsOnRuleId"];
        } else if (j["dependsOnRuleId"].is_string()) {
            rule->dependsOnRuleId = std::stoi(j["dependsOnRuleId"].get<std::string>());
        }
    }

    if (j.contains("timeout") && !j["timeout"].is_null()) {
        if (j["timeout"].is_number()) {
            rule->timeout = std::chrono::milliseconds(j["timeout"]);
        } else if (j["timeout"].is_string()) {
            // Parse duration string like "1000ms", "5s", "1m"
            std::string ts = j["timeout"];
            if (ts.ends_with("ms")) {
                rule->timeout = std::chrono::milliseconds(std::stoll(ts.substr(0, ts.size() - 2)));
            } else if (ts.ends_with("s")) {
                rule->timeout = std::chrono::seconds(std::stoll(ts.substr(0, ts.size() - 1)));
            } else if (ts.ends_with("m")) {
                rule->timeout = std::chrono::minutes(std::stoll(ts.substr(0, ts.size() - 1)));
            }
        }
    }

    if (j.contains("cacheDuration") && !j["cacheDuration"].is_null()) {
        if (j["cacheDuration"].is_number()) {
            rule->cacheDuration = std::chrono::milliseconds(j["cacheDuration"]);
        } else if (j["cacheDuration"].is_string()) {
            std::string ts = j["cacheDuration"];
            if (ts.ends_with("ms")) {
                rule->cacheDuration = std::chrono::milliseconds(std::stoll(ts.substr(0, ts.size() - 2)));
            } else if (ts.ends_with("s")) {
                rule->cacheDuration = std::chrono::seconds(std::stoll(ts.substr(0, ts.size() - 1)));
            } else if (ts.ends_with("m")) {
                rule->cacheDuration = std::chrono::minutes(std::stoll(ts.substr(0, ts.size() - 1)));
            }
        }
    }

    if (j.contains("childRules") && j["childRules"].is_array()) {
        for (const auto& child : j["childRules"]) {
            auto childRule = parseRule(child);
            childRule->parentRule = rule;
            rule->childRules.push_back(childRule);
        }
    }

    return rule;
}

nlohmann::json JsonLoader::serializeRule(const Rule& rule) {
    nlohmann::json j;
    j["id"] = rule.id;
    j["description"] = rule.description;
    j["isActive"] = rule.isActive;
    j["priority"] = rule.priority;
    j["expression"] = rule.expression;
    j["action"] = rule.action;

    if (rule.dependsOnRuleId.has_value()) {
        j["dependsOnRuleId"] = rule.dependsOnRuleId.value();
    } else {
        j["dependsOnRuleId"] = nullptr;
    }

    if (rule.timeout.has_value()) {
        j["timeout"] = rule.timeout->count();
    } else {
        j["timeout"] = nullptr;
    }

    if (rule.cacheDuration.has_value()) {
        j["cacheDuration"] = rule.cacheDuration->count();
    } else {
        j["cacheDuration"] = nullptr;
    }

    j["childRules"] = nlohmann::json::array();
    for (const auto& child : rule.childRules) {
        j["childRules"].push_back(serializeRule(*child));
    }

    return j;
}

Workflow JsonLoader::loadWorkflow(const std::string& jsonString) {
    Workflow workflow;

    nlohmann::json j = nlohmann::json::parse(jsonString);

    if (j.contains("id")) {
        if (j["id"].is_number_integer()) {
            workflow.id = j["id"];
        } else if (j["id"].is_string()) {
            workflow.id = std::stoi(j["id"].get<std::string>());
        } else {
            static int counter = 0;
            workflow.id = ++counter;
        }
    } else {
        static int counter = 0;
        workflow.id = ++counter;
    }

    if (j.contains("description") && j["description"].is_string()) {
        workflow.description = j["description"];
    }

    if (j.contains("isActive") && j["isActive"].is_boolean()) {
        workflow.isActive = j["isActive"];
    }

    if (j.contains("rules") && j["rules"].is_array()) {
        for (const auto& ruleJson : j["rules"]) {
            workflow.rules.push_back(parseRule(ruleJson));
        }
    }

    return workflow;
}

Workflow JsonLoader::loadWorkflowFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw RuleException("Cannot open file: " + filePath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadWorkflow(buffer.str());
}

std::shared_ptr<Rule> JsonLoader::loadRule(const std::string& jsonString) {
    nlohmann::json j = nlohmann::json::parse(jsonString);
    return parseRule(j);
}

std::string JsonLoader::saveWorkflow(const Workflow& workflow) {
    nlohmann::json j;
    j["id"] = workflow.id;
    j["description"] = workflow.description;
    j["isActive"] = workflow.isActive;
    j["rules"] = nlohmann::json::array();

    for (const auto& rule : workflow.rules) {
        j["rules"].push_back(serializeRule(*rule));
    }

    return j.dump();
}

std::string JsonLoader::saveWorkflowPretty(const Workflow& workflow) {
    nlohmann::json j;
    j["id"] = workflow.id;
    j["description"] = workflow.description;
    j["isActive"] = workflow.isActive;
    j["rules"] = nlohmann::json::array();

    for (const auto& rule : workflow.rules) {
        j["rules"].push_back(serializeRule(*rule));
    }

    return j.dump(2);
}

std::string JsonLoader::saveRule(const Rule& rule) {
    return serializeRule(rule).dump();
}

std::string JsonLoader::saveRulePretty(const Rule& rule) {
    return serializeRule(rule).dump(2);
}

} // namespace fastrules
