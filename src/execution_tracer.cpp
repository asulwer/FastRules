#include "fastrules/execution_tracer.hpp"
#include <algorithm>

namespace fastrules {

ExecutionTracer::ExecutionTracer(std::string workflowId) {
    trace_.workflowId = std::move(workflowId);
}

void ExecutionTracer::start() {
    trace_.startedAt = std::chrono::steady_clock::now();
    active_ = true;
}

void ExecutionTracer::addStep(ExecutionTraceStep step) {
    if (active_) {
        trace_.steps.push_back(std::move(step));
    }
}

void ExecutionTracer::record(const std::string& ruleId,
                             const std::string& stage,
                             bool success,
                             const std::optional<std::string>& message,
                             const std::optional<std::string>& dependencyId) {
    if (!active_) return;

    ExecutionTraceStep step;
    step.ruleId = ruleId;
    step.stage = stage;
    step.success = success;
    step.startedAt = std::chrono::steady_clock::now();
    step.endedAt = step.startedAt;  // Instant step unless timed separately
    step.message = message;
    step.dependencyId = dependencyId;
    trace_.steps.push_back(std::move(step));
}

void ExecutionTracer::finish(bool overallSuccess) {
    trace_.endedAt = std::chrono::steady_clock::now();
    trace_.overallSuccess = overallSuccess;
    active_ = false;
}

std::vector<ExecutionTraceStep> ExecutionTrace::getStepsForRule(const std::string& ruleId) const {
    std::vector<ExecutionTraceStep> result;
    for (const auto& step : steps) {
        if (step.ruleId == ruleId) {
            result.push_back(step);
        }
    }
    return result;
}

std::chrono::nanoseconds ExecutionTrace::getTotalTimeInStage(const std::string& stage) const {
    std::chrono::nanoseconds total{0};
    for (const auto& step : steps) {
        if (step.stage == stage) {
            total += step.duration();
        }
    }
    return total;
}

std::optional<ExecutionTraceStep> ExecutionTrace::getSlowestStep() const {
    if (steps.empty()) return std::nullopt;

    auto it = std::max_element(steps.begin(), steps.end(),
        [](const auto& a, const auto& b) {
            return a.duration() < b.duration();
        });
    return *it;
}

} // namespace fastrules
