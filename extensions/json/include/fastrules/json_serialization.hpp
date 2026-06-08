#pragma once

#include <fastrules/rule_versioning.hpp>
#include <fastrules/execution_tracer.hpp>
#include <fastrules/performance_counters.hpp>
#include <fastrules/workflow.hpp>
#include <fastrules/rule.hpp>
#include <string>
#include <nlohmann/json.hpp>

namespace fastrules {
namespace ext {

/**
 * JSON serialization helpers for core types.
 * These replace the toJson() methods that were removed from core.
 */
class JsonSerialization {
public:
    // RuleVersionHistory
    [[nodiscard]] static std::string serialize(const RuleVersionHistory& history);
    [[nodiscard]] static std::optional<RuleVersionHistory> deserializeRuleVersionHistory(const std::string& json);
    
    // RuleVersionManager
    [[nodiscard]] static std::string serialize(const RuleVersionManager& manager);
    static void deserialize(RuleVersionManager& manager, const std::string& json);
    
    // ExecutionTrace
    [[nodiscard]] static std::string serialize(const ExecutionTrace& trace);
    [[nodiscard]] static std::optional<ExecutionTrace> deserializeExecutionTrace(const std::string& json);
    
    // PerformanceCounters
    [[nodiscard]] static std::string serialize(const PerformanceCounters& counters);
    
    // Workflow
    [[nodiscard]] static std::string serialize(const Workflow& workflow);
    [[nodiscard]] static Workflow deserializeWorkflow(const std::string& json);
    
    // Rule
    [[nodiscard]] static std::string serialize(const Rule& rule);
    [[nodiscard]] static std::shared_ptr<Rule> deserializeRule(const std::string& json);
};

} // namespace ext
} // namespace fastrules
