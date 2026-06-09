#include "fastrules/workflow.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/streaming_result.hpp"
#include "fastrules/logger.hpp"

#include <algorithm>
#include <stdexcept>
#include <queue>
#include <unordered_set>
#include <future>
#include <thread>
#include <mutex>

namespace fastrules {

void Workflow::validate() {
    auto log = fastrules::logger();
    if (validated_) {
        return;
    }

    log->debug("Validating workflow {}", id);

    // Check for duplicate rule IDs
    std::unordered_set<int> seenIds;
    for (const auto& rule : rules) {
        if (rule->id != 0) {
            if (seenIds.contains(rule->id)) {
                log->error("Duplicate rule ID detected: {}", rule->id);
                throw RuleValidationException("Duplicate rule ID detected: " + std::to_string(rule->id));
            }
            seenIds.insert(rule->id);
        }
    }

    // Collect all rules for validation without copying
    std::vector<std::reference_wrapper<const Rule>> allRules;
    allRules.reserve(rules.size());
    for (const auto& rule : rules) {
        allRules.emplace_back(*rule);
    }

    // Validate each rule
    for (auto& rule : rules) {
        rule->validate(allRules);
    }

    // Check for circular dependencies
    checkCircularDependencies();

    log->info("Workflow {} validated successfully", id);
    validated_ = true;
}

void Workflow::compile(LuaEngine& engine) {
    auto& log = globalLogger();
    if (compiled_) {
        return;
    }

    if (!validated_) {
        validate();
    }

    log->debug("Compiling workflow {}", id);

    // Auto-discover callbacks from actions before compiling
    // This ensures any callbacks referenced in JSON actions get stub registrations
    auto actions = getAllActions();
    if (!actions.empty()) {
        engine.discoverCallbacks(actions);
    }

    // Compile each rule
    for (auto& rule : rules) {
        rule->compile(engine);
    }

    log->info("Workflow {} compiled successfully", id);
    compiled_ = true;
}

bool Workflow::isCompiled() const noexcept {
    return compiled_;
}

std::vector<RuleResult> Workflow::execute(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    auto log = fastrules::logger();
    if (!compiled_) {
        compile(engine);
    }

    log->debug("Executing workflow {}", id);

    RuleContext context;
    std::vector<RuleResult> results;

    auto executionOrder = resolveExecutionOrder();
    log->info("Executing {} rules in workflow {}", executionOrder.size(), id);

    for (auto& rule : executionOrder) {
        // Preference: skip inactive rules entirely - no evaluation, no result
        if (!rule->isActive) {
            continue;
        }

        // Check dependency: if rule depends on another, ensure it succeeded
        if (rule->dependsOnRuleId.has_value()) {
            auto depResult = context.getResult(rule->dependsOnRuleId.value());
            if (!depResult.has_value() || !depResult->isSuccess()) {
                log->debug("Skipping rule {} — dependency failed in workflow {}", rule->id, id);
                // Dependency failed - skip this rule silently
                continue;
            }
        }

        auto result = rule->execute(engine, context, parameters);

        // Preference: workflow is a holder for all rules, return only parent results
        // Skip adding child results - they're internal to parent evaluation
        if (!result.skipped) {
            results.push_back(result);
        }
    }

    log->info("Workflow {} executed — {} results", id, results.size());
    return results;
}

std::vector<RuleResult> Workflow::executeWithTrace(LuaEngine& engine,
    const std::vector<RuleParameter>& parameters,
    ExecutionTracer& tracer) {
    if (!compiled_) {
        compile(engine);
    }

    tracer.start();

    RuleContext context;
    std::vector<RuleResult> results;

    auto executionOrder = resolveExecutionOrder();

    for (auto& rule : executionOrder) {
        // Preference: skip inactive rules entirely - no evaluation, no result
        if (!rule->isActive) {
            tracer.record(rule->id, "skip", true, "Rule is inactive");
            continue;
        }

        // Check dependency
        if (rule->dependsOnRuleId.has_value()) {
            ExecutionTraceStep depStep;
            depStep.ruleId = rule->id;
            depStep.stage = "dependency_check";
            depStep.dependencyId = rule->dependsOnRuleId.value();
            depStep.startedAt = std::chrono::steady_clock::now();

            auto depResult = context.getResult(rule->dependsOnRuleId.value());
            depStep.success = depResult.has_value() && depResult->isSuccess();
            depStep.endedAt = std::chrono::steady_clock::now();

            if (!depResult.has_value() || !depResult->isSuccess()) {
                depStep.message = "Dependency '" + rule->dependsOnRuleId.value() + "' failed or not found";
                tracer.addStep(std::move(depStep));
                continue;
            }
            depStep.message = "Dependency '" + rule->dependsOnRuleId.value() + "' satisfied";
            tracer.addStep(std::move(depStep));
        }

        // Execute rule with timing
        ExecutionTraceStep execStep;
        execStep.ruleId = rule->id;
        execStep.stage = "execute";
        execStep.expression = rule->expression;
        execStep.action = rule->action;
        execStep.startedAt = std::chrono::steady_clock::now();

        auto result = rule->execute(engine, context, parameters);
        execStep.success = result.isSuccess();
        execStep.endedAt = std::chrono::steady_clock::now();

        if (result.exception.has_value()) {
            execStep.message = result.exception->what();
        } else if (result.skipped) {
            execStep.message = "Rule was skipped";
        } else {
            execStep.message = result.success ? "Rule evaluated to true" : "Rule evaluated to false";
        }

        tracer.addStep(std::move(execStep));

        context.setResult(rule->id, result);

        if (!result.skipped) {
            results.push_back(result);
        }
    }

    bool overallSuccess = !results.empty() && std::all_of(results.begin(), results.end(),
        [](const auto& r) { return r.isSuccess(); });
    tracer.finish(overallSuccess);

    return results;
}

std::vector<RuleResult> Workflow::executeParallel(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    if (!compiled_) {
        compile(engine);
    }

    RuleContext context;
    std::vector<RuleResult> results;
    std::mutex resultsMutex;

    // Build dependency levels
    auto dependencyLevels = buildDependencyLevels();

    // Execute each level in parallel
    for (const auto& level : dependencyLevels) {
        std::vector<std::future<std::pair<std::string, RuleResult>>> futures;

        for (const auto& rule : level) {
            if (!rule->isActive) {
                continue;
            }

            futures.push_back(
                std::async(std::launch::async, [&engine, &context, &parameters, rule]() {
                    // Check dependency before execution
                    if (rule->dependsOnRuleId.has_value()) {
                        auto depResult = context.getResult(rule->dependsOnRuleId.value());
                        if (!depResult.has_value() || !depResult->isSuccess()) {
                            RuleResult skipResult;
                            skipResult.ruleId = rule->id;
                            skipResult.success = false;
                            skipResult.exception = RuleException("Dependency failed: " + rule->dependsOnRuleId.value());
                            return std::make_pair(rule->id, skipResult);
                        }
                    }

                    // Use the engine directly (it's thread-safe for concurrent reads via shared_lock)
                    auto result = rule->execute(engine, context, parameters);
                    return std::make_pair(rule->id, result);
                })
            );
        }

        // Collect results from this level
        for (auto& future : futures) {
            auto [ruleId, result] = future.get();

            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(result);

            // Add to context for dependent rules
            context.setResult(ruleId, result);
        }
    }

    return results;
}

StreamingResult Workflow::executeStreaming(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    if (!compiled_) {
        compile(engine);
    }

    // Capture workflow state by value for the generator
    auto rulesCopy = rules;

    return StreamingResult([this, &engine, parameters, rulesCopy]() mutable -> std::optional<RuleResult> {
        // Use a persistent context and index stored in the closure
        static thread_local RuleContext* ctx = nullptr;
        static thread_local size_t idx = 0;
        static thread_local bool initialized = false;

        if (!initialized) {
            ctx = new RuleContext();
            idx = 0;
            initialized = true;
        }

        // Find next rule to execute
        while (idx < rulesCopy.size()) {
            auto& rule = rulesCopy[idx++];

            if (!rule->isActive) {
                continue;
            }

            // Check dependency
            if (rule->dependsOnRuleId.has_value()) {
                auto depResult = ctx->getResult(rule->dependsOnRuleId.value());
                if (!depResult.has_value() || !depResult->isSuccess()) {
                    continue;
                }
            }

            auto result = rule->execute(engine, *ctx, parameters);
            ctx->setResult(rule->id, result);

            if (!result.skipped) {
                return result;
            }
        }

        // Cleanup on end
        delete ctx;
        ctx = nullptr;
        initialized = false;
        return std::nullopt;
    });
}

std::vector<std::shared_ptr<Rule>> Workflow::resolveExecutionOrder() const {
    // Build dependency levels via topological sort
    auto levels = buildDependencyLevels();

    std::vector<std::shared_ptr<Rule>> order;
    for (const auto& level : levels) {
        for (const auto& rule : level) {
            order.push_back(rule);
        }
    }
    return order;
}

std::vector<std::vector<std::shared_ptr<Rule>>> Workflow::buildDependencyLevels() const {
    // Kahn's algorithm for topological sort with priority within levels
    std::unordered_map<std::string, std::shared_ptr<Rule>> ruleMap;
    std::unordered_map<std::string, int> inDegree;

    for (const auto& rule : rules) {
        ruleMap[rule->id] = rule;
        inDegree[rule->id] = 0;
    }

    // Count dependencies
    for (const auto& rule : rules) {
        if (rule->dependsOnRuleId.has_value() && ruleMap.contains(rule->dependsOnRuleId.value())) {
            inDegree[rule->id]++;
        }
    }

    std::vector<std::vector<std::shared_ptr<Rule>>> levels;

    while (!ruleMap.empty()) {
        std::vector<std::shared_ptr<Rule>> level;

        // Find all rules with in-degree 0
        for (const auto& [id, rule] : ruleMap) {
            if (inDegree[id] == 0) {
                level.push_back(rule);
            }
        }

        if (level.empty() && !ruleMap.empty()) {
            throw RuleValidationException("Circular dependency detected in workflow");
        }

        // Sort level by priority
        std::sort(level.begin(), level.end(), [](const auto& a, const auto& b) {
            return a->priority < b->priority;
        });

        // Remove processed rules and update in-degrees
        for (const auto& rule : level) {
            ruleMap.erase(rule->id);

            // Decrease in-degree for rules depending on this one
            for (const auto& [id, r] : ruleMap) {
                (void)id;
                if (r->dependsOnRuleId == rule->id) {
                    inDegree[r->id]--;
                }
            }
        }

        if (!level.empty()) {
            levels.push_back(level);
        }
    }
    return levels;
}

void Workflow::checkCircularDependencies() const {
    // DFS-based cycle detection
    std::unordered_map<std::string, std::shared_ptr<Rule>> ruleMap;
    for (const auto& rule : rules) {
        ruleMap[rule->id] = rule;
    }

    enum class VisitState { Unvisited, Visiting, Visited };
    std::unordered_map<std::string, VisitState> state;

    for (const auto& [id, _] : ruleMap) {
        state[id] = VisitState::Unvisited;
    }

    std::function<bool(const std::string&)> dfs = [&](const std::string& ruleId) -> bool {
        state[ruleId] = VisitState::Visiting;

        auto it = ruleMap.find(ruleId);
        if (it != ruleMap.end() && it->second->dependsOnRuleId.has_value()) {
            const std::string& depId = it->second->dependsOnRuleId.value();
            if (state[depId] == VisitState::Visiting) {
                return true; // Cycle found
            }
            if (state[depId] == VisitState::Unvisited && dfs(depId)) {
                return true;
            }
        }

        state[ruleId] = VisitState::Visited;
        return false;
    };

    for (const auto& [id, _] : ruleMap) {
        if (state[id] == VisitState::Unvisited && dfs(id)) {
            throw RuleValidationException("Circular dependency detected");
        }
    }
}

std::vector<std::string> Workflow::getAllActions() const {
    std::vector<std::string> actions;
    collectActions(rules, actions);
    return actions;
}

void Workflow::collectActions(const std::vector<std::shared_ptr<Rule>>& ruleList, std::vector<std::string>& out) const {
    for (const auto& rule : ruleList) {
        if (!rule->action.empty()) {
            out.push_back(rule->action);
        }
        if (!rule->childRules.empty()) {
            collectActions(rule->childRules, out);
        }
    }
}

// ============================================================================
// Workflow Builder implementation
// ============================================================================

Workflow::Builder::Builder(const std::string& workflowId)
    : workflow_(std::make_unique<Workflow>()) {
    workflow_->id = workflowId;
}

Workflow::Builder& Workflow::Builder::withDescription(const std::string& desc) {
    workflow_->description = desc;
    return *this;
}

Workflow::Builder& Workflow::Builder::addRule(std::shared_ptr<Rule> rule) {
    workflow_->rules.push_back(std::move(rule));
    return *this;
}

Workflow::Builder& Workflow::Builder::active(bool active) {
    workflow_->isActive = active;
    return *this;
}

Workflow Workflow::Builder::build() {
    return std::move(*workflow_);
}

} // namespace fastrules
