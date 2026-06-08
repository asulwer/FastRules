#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <optional>

namespace fastrules {

// A single step in the execution trace
struct ExecutionTraceStep {
    std::string ruleId;
    std::string stage;           // "compile", "evaluate", "action", "skip", "dependency_check"
    bool success = true;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point endedAt;
    std::optional<std::string> message;       // Human-readable detail
    std::optional<std::string> dependencyId; // For dependency_check stage
    std::optional<std::string> expression;    // The expression being evaluated
    std::optional<std::string> action;      // The action being executed

    [[nodiscard]] std::chrono::nanoseconds duration() const {
        return endedAt - startedAt;
    }
};

// Complete trace for a workflow execution
struct ExecutionTrace {
    std::string workflowId;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point endedAt;
    std::vector<ExecutionTraceStep> steps;
    bool overallSuccess = false;

    [[nodiscard]] std::chrono::nanoseconds totalDuration() const {
        return endedAt - startedAt;
    }

    // Get steps for a specific rule
    [[nodiscard]] std::vector<ExecutionTraceStep> getStepsForRule(const std::string& ruleId) const;

    // Get total time spent in a specific stage across all rules
    [[nodiscard]] std::chrono::nanoseconds getTotalTimeInStage(const std::string& stage) const;

    // Get the slowest step
    [[nodiscard]] std::optional<ExecutionTraceStep> getSlowestStep() const;
};

// Lightweight tracer that collects steps during execution
class ExecutionTracer {
public:
    ExecutionTracer() = default;
    explicit ExecutionTracer(std::string workflowId);

    // Start the trace
    void start();

    // Record a step
    void addStep(ExecutionTraceStep step);

    // Convenience: record with auto timestamps
    void record(const std::string& ruleId,
                const std::string& stage,
                bool success = true,
                const std::optional<std::string>& message = std::nullopt,
                const std::optional<std::string>& dependencyId = std::nullopt);

    // Mark trace as complete
    void finish(bool overallSuccess);

    // Access the trace
    [[nodiscard]] const ExecutionTrace& getTrace() const { return trace_; }
    [[nodiscard]] ExecutionTrace takeTrace() && { return std::move(trace_); }

    // Check if tracing is active
    [[nodiscard]] bool isActive() const { return active_; }

private:
    ExecutionTrace trace_;
    bool active_ = false;
};

} // namespace fastrules
