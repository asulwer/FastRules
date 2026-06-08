#pragma once

#include <string>
#include <vector>
#include <memory>

#include <nlohmann/json.hpp>

#include <fastrules/workflow.hpp>
#include <fastrules/rule.hpp>

namespace fastrules {

// JSON serialization/deserialization for workflows and rules
class JsonLoader {
public:
    // Load workflow from JSON string
    [[nodiscard]] static Workflow loadWorkflow(const std::string& jsonString);

    // Load workflow from JSON file
    [[nodiscard]] static Workflow loadWorkflowFromFile(const std::string& filePath);

    // Load a single rule from JSON string
    [[nodiscard]] static std::shared_ptr<Rule> loadRule(const std::string& jsonString);

    // Serialize workflow to JSON
    [[nodiscard]] static std::string saveWorkflow(const Workflow& workflow);
    [[nodiscard]] static std::string saveWorkflowPretty(const Workflow& workflow);

    // Serialize rule to JSON
    [[nodiscard]] static std::string saveRule(const Rule& rule);
    [[nodiscard]] static std::string saveRulePretty(const Rule& rule);

private:
    [[nodiscard]] static std::shared_ptr<Rule> parseRule(const nlohmann::json& j);
    [[nodiscard]] static nlohmann::json serializeRule(const Rule& rule);
};

} // namespace fastrules
