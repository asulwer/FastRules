#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <filesystem>
#include <mutex>

#include "lua_engine.hpp"
#include "rule.hpp"
#include "rule_result.hpp"
#include "streaming_result.hpp"
#include "execution_tracer.hpp"

namespace fastrules {

// Forward declarations (now redundant due to lua_engine.hpp include, but kept for clarity)
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
    using Id = int;

    Workflow() = default;
    ~Workflow();  // Defined in .cpp where LuaEngine is complete
    
    // Non-copyable (due to std::mutex), but movable
    Workflow(const Workflow&) = delete;
    Workflow& operator=(const Workflow&) = delete;
    Workflow(Workflow&&) = default;
    Workflow& operator=(Workflow&&) = default;

    // Properties
    Id id = 0;
    std::string description;
    bool isActive = true;
    std::vector<std::shared_ptr<Rule>> rules;

    // Validation and compilation
    void validate();
    void compile(LuaEngine& engine);
    [[nodiscard]] bool isCompiled() const noexcept;

    // Execution modes
    // Synchronous sequential execution - rules execute one at a time
    [[nodiscard]] std::vector<RuleResult> execute(LuaEngine& engine, const std::vector<RuleParameter>& parameters);
    
    // Parallel execution - rules in each dependency level execute concurrently
    // NOTE: Creates thread pool per execution. For repeated execution, use AsyncWorkflow.
    // See docs/parallel-execution.md for executeParallel vs AsyncWorkflow guidance.
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

    // Builder pattern for fluent construction
    class Builder {
    public:
        explicit Builder(int workflowId);

        Builder& withDescription(const std::string& desc);
        Builder& addRule(std::shared_ptr<Rule> rule);
        Builder& active(bool active = true);

        [[nodiscard]] Workflow build();

    private:
        std::unique_ptr<Workflow> workflow_;
    };

    // Static builder entry point
    [[nodiscard]] static Builder builder(int workflowId) {
        return Builder(workflowId);
    }

private:
    bool compiled_ = false;
    bool validated_ = false;

    // Engine clone pool for parallel execution optimization
    // Pre-created and pre-compiled clones avoid per-task allocation overhead
    std::vector<std::unique_ptr<LuaEngine>> enginePool_;
    std::unique_ptr<std::mutex> poolMutex_;
    size_t poolNextIndex_ = 0;

    // Helper: topological sort for dependency-aware execution
    [[nodiscard]] std::vector<std::vector<std::shared_ptr<Rule>>> buildDependencyLevels() const;

    // Helper: detect circular dependencies
    void checkCircularDependencies() const;

    // Helper: recursively collect actions from rules and child rules
    void collectActions(const std::vector<std::shared_ptr<Rule>>& ruleList, std::vector<std::string>& out) const;
    
    // Helper: get next available engine from the pool (round-robin)
    LuaEngine* acquireEngine();
    void releaseEngine(LuaEngine* engine);

    // Dependency graph visualization
    // Returns a DOT format graph string for debugging/visualization
    // Usage: workflow.toGraph() | dot -Tpng > workflow.png
    [[nodiscard]] std::string toGraph() const;
};

} // namespace fastrules
