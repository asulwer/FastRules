#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

#include <optional>

#include "rule.hpp"
#include "rule_result.hpp"
#include "streaming_result.hpp"
#include "execution_tracer.hpp"

#include <optional>
#include <filesystem>

#ifdef FASTRULES_HAS_JSON_LOADER
namespace fastrules {
    class JsonLoader;
}
#endif

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;

// Execution strategy enum
enum class ExecutionMode {
    Sequential,        // Execute rules one by one in order
    Parallel,          // Execute independent rules in parallel
    Priority,          // Order by priority, respecting dependencies
    DependencyGraph    // Topological sort, parallel per level
};

class Workflow {
public:
    using Id = std::string;

    Workflow() = default;
    ~Workflow() = default;

    // Properties
    Id id;
    std::string description;
    bool isActive = true;
    std::vector<std::shared_ptr<Rule>> rules;

    // Validation and compilation
    void validate();
    void compile(LuaEngine& engine);
    [[nodiscard]] bool isCompiled() const noexcept;

    // Execution modes
    [[nodiscard]] std::vector<RuleResult> execute(LuaEngine& engine, const std::vector<RuleParameter>& parameters);
    [[nodiscard]] std::vector<RuleResult> executeParallel(LuaEngine& engine, const std::vector<RuleParameter>& parameters);

    // Streaming execution — yields results as they complete
    [[nodiscard]] StreamingResult executeStreaming(LuaEngine& engine, const std::vector<RuleParameter>& parameters);

    // Execution with tracing
    [[nodiscard]] std::vector<RuleResult> executeWithTrace(LuaEngine& engine,
        const std::vector<RuleParameter>& parameters,
        ExecutionTracer& tracer);

    // Dependency resolution
    [[nodiscard]] std::vector<std::shared_ptr<Rule>> resolveExecutionOrder() const;

    // Get all action strings from all rules (for callback discovery)
    [[nodiscard]] std::vector<std::string> getAllActions() const;

    // Convenience: load workflow from JSON string
    // Requires the JSON extension (fastrules-json). If unavailable, returns std::nullopt.
    [[nodiscard]] static std::optional<Workflow> fromJson(const std::string& jsonString) {
        // Requires the JSON extension (fastrules-json).
        // Call JsonLoader::loadWorkflow directly from code that links fastrules-json.
        (void)jsonString;
        return std::nullopt;
    }

    // Builder pattern for fluent construction
    class Builder {
    public:
        explicit Builder(const std::string& workflowId);

        Builder& withDescription(const std::string& desc);
        Builder& addRule(std::shared_ptr<Rule> rule);
        Builder& active(bool active = true);

        [[nodiscard]] Workflow build();

    private:
        std::unique_ptr<Workflow> workflow_;
    };

    // Static builder entry point
    [[nodiscard]] static Builder builder(const std::string& workflowId) {
        return Builder(workflowId);
    }

private:
    bool compiled_ = false;
    bool validated_ = false;

    // Helper: topological sort for dependency-aware execution
    [[nodiscard]] std::vector<std::vector<std::shared_ptr<Rule>>> buildDependencyLevels() const;

    // Helper: detect circular dependencies
    void checkCircularDependencies() const;

    // Helper: recursively collect actions from rules and child rules
    void collectActions(const std::vector<std::shared_ptr<Rule>>& ruleList, std::vector<std::string>& out) const;
};

} // namespace fastrules
