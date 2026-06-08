#pragma once

#include <string>
#include <memory>

#include <fastrules/workflow.hpp>
#include <fastrules/rule.hpp>

namespace fastrules {

// XML serialization/deserialization for workflows and rules
class XmlLoader {
public:
    // Load workflow from XML string
    [[nodiscard]] static Workflow loadWorkflow(const std::string& xmlString);

    // Load workflow from XML file
    [[nodiscard]] static Workflow loadWorkflowFromFile(const std::string& filePath);

    // Load a single rule from XML string
    [[nodiscard]] static std::shared_ptr<Rule> loadRule(const std::string& xmlString);

    // Serialize workflow to XML
    [[nodiscard]] static std::string saveWorkflow(const Workflow& workflow);
    [[nodiscard]] static std::string saveWorkflowPretty(const Workflow& workflow);

    // Serialize rule to XML
    [[nodiscard]] static std::string saveRule(const Rule& rule);
    [[nodiscard]] static std::string saveRulePretty(const Rule& rule);
};

} // namespace fastrules
