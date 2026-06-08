#pragma once

#include <fastrules/rule_versioning.hpp>
#include <fastrules/execution_tracer.hpp>
#include <fastrules/performance_counters.hpp>
#include <fastrules/workflow.hpp>
#include <fastrules/rule.hpp>
#include <string>

namespace fastrules {
namespace ext {

/**
 * XML serialization helpers for core types.
 * Mirrors JsonSerialization for parity.
 */
class XmlSerialization {
public:
    // RuleVersionHistory
    [[nodiscard]] static std::string serialize(const RuleVersionHistory& history);
    [[nodiscard]] static std::optional<RuleVersionHistory> deserializeRuleVersionHistory(const std::string& xml);

    // RuleVersionManager
    [[nodiscard]] static std::string serialize(const RuleVersionManager& manager);
    static void deserialize(RuleVersionManager& manager, const std::string& xml);

    // ExecutionTrace
    [[nodiscard]] static std::string serialize(const ExecutionTrace& trace);

    // PerformanceCounters
    [[nodiscard]] static std::string serialize(const PerformanceCounters& counters);

    // Workflow / Rule — delegates to XmlLoader
    [[nodiscard]] static std::string serialize(const Workflow& workflow);
    [[nodiscard]] static Workflow deserializeWorkflow(const std::string& xml);

    [[nodiscard]] static std::string serialize(const Rule& rule);
    [[nodiscard]] static std::shared_ptr<Rule> deserializeRule(const std::string& xml);
};

} // namespace ext
} // namespace fastrules
