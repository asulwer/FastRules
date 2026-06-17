/**
 * @file execution_tracer.cpp
 * @brief Execution tracing for debugging and performance analysis
 * 
 * The ExecutionTracer captures a detailed trace of workflow execution,
 * including timing information for each rule at each stage. This is
 * invaluable for debugging rule failures and performance optimization.
 * 
 * Tracing Model:
 * - Tracer is created for a specific workflow execution
 * - Steps are recorded as rules progress through execution stages
 * - Each step captures: rule name, stage, timing, success/failure
 * - The trace can be analyzed after execution completes
 * 
 * Stages Traced:
 * - "start": Rule execution begins
 * - "dependency_check": Checking if dependencies passed
 * - "expression_eval": Evaluating the rule expression
 * - "action_exec": Executing the rule action
 * - "complete": Rule execution finished
 * 
 * Usage:
 * @code
 * ExecutionTracer tracer(workflow.id);
 * tracer.start();
 * 
 * // During execution
 * tracer.record("rule1", "expression_eval", true, std::nullopt, std::nullopt);
 * 
 * // After execution
 * tracer.finish(overallSuccess);
 * 
 * // Analysis
 * auto slowest = tracer.getTrace().getSlowestStep();
 * auto totalEvalTime = tracer.getTrace().getTotalTimeInStage("expression_eval");
 * @endcode
 * 
 * Performance Impact:
 * - Tracing adds overhead (~100ns per step)
 * - Only trace when needed (debugging, profiling)
 * - Disable in production for maximum performance
 */

#include "fastrules/execution_tracer.hpp"
#include <algorithm>

namespace fastrules {

/**
 * @brief Construct a tracer for a workflow
 * 
 * Initializes the trace with the workflow ID. Tracing is inactive
 * until start() is called.
 * 
 * @param workflowId ID of the workflow being traced
 */
ExecutionTracer::ExecutionTracer(int workflowId) {
    trace_.workflowId = workflowId;
}

/**
 * @brief Start tracing
 * 
 * Records the start timestamp and activates tracing.
 * Steps recorded before start() are ignored.
 */
void ExecutionTracer::start() {
    trace_.startedAt = std::chrono::steady_clock::now();
    active_ = true;
}

/**
 * @brief Add a pre-constructed step to the trace
 * 
 * Used when the step has been prepared externally.
 * Only adds the step if tracing is active.
 * 
 * @param step The step to add
 */
void ExecutionTracer::addStep(ExecutionTraceStep step) {
    if (active_) {
        trace_.steps.push_back(std::move(step));
    }
}

/**
 * @brief Record a step with full details
 * 
 * Convenience method for creating and adding a step in one call.
 * This is the primary method for recording execution steps.
 * 
 * @param ruleName Name of the rule being traced
 * @param stage The execution stage (e.g., "expression_eval")
 * @param success Whether this stage succeeded
 * @param message Optional message (error text, etc.)
 * @param dependencyId Optional dependency rule ID
 */
void ExecutionTracer::record(const std::string& ruleName,
                             const std::string& stage,
                             bool success,
                             const std::optional<std::string>& message,
                             const std::optional<int>& dependencyId) {
    if (!active_) return;

    ExecutionTraceStep step;
    step.ruleName = ruleName;
    step.stage = stage;
    step.success = success;
    step.startedAt = std::chrono::steady_clock::now();
    step.endedAt = step.startedAt;  // Same timestamp for instant steps
    step.message = message;
    step.dependencyId = dependencyId;
    trace_.steps.push_back(std::move(step));
}

/**
 * @brief Finish tracing
 * 
 * Records the end timestamp and overall success status.
 * Deactivates tracing - subsequent record() calls are ignored.
 * 
 * @param overallSuccess Whether the entire workflow succeeded
 */
void ExecutionTracer::finish(bool overallSuccess) {
    trace_.endedAt = std::chrono::steady_clock::now();
    trace_.overallSuccess = overallSuccess;
    active_ = false;
}

// ============================================================================
// ExecutionTrace Analysis Methods
// ============================================================================

/**
 * @brief Get all steps for a specific rule
 * 
 * Useful for analyzing the execution history of a single rule
 * across all its stages.
 * 
 * @param ruleName Name of the rule to filter by
 * @return Vector of steps for that rule
 */
std::vector<ExecutionTraceStep> ExecutionTrace::getStepsForRule(const std::string& ruleName) const {
    std::vector<ExecutionTraceStep> result;
    for (const auto& step : steps) {
        if (step.ruleName == ruleName) {
            result.push_back(step);
        }
    }
    return result;
}

/**
 * @brief Calculate total time spent in a stage
 * 
 * Sums the durations of all steps with the given stage.
 * Useful for identifying which stages consume the most time.
 * 
 * @param stage The stage name to total (e.g., "expression_eval")
 * @return Total time spent in that stage
 */
std::chrono::nanoseconds ExecutionTrace::getTotalTimeInStage(const std::string& stage) const {
    std::chrono::nanoseconds total{0};
    for (const auto& step : steps) {
        if (step.stage == stage) {
            total += step.duration();
        }
    }
    return total;
}

/**
 * @brief Find the slowest step in the trace
 * 
 * Uses std::max_element to find the step with the longest duration.
 * Returns std::nullopt if the trace has no steps.
 * 
 * @return The slowest step, or std::nullopt if empty
 */
std::optional<ExecutionTraceStep> ExecutionTrace::getSlowestStep() const {
    if (steps.empty()) return std::nullopt;

    auto it = std::max_element(steps.begin(), steps.end(),
        [](const auto& a, const auto& b) {
            return a.duration() < b.duration();
        });
    return *it;
}

} // namespace fastrules
