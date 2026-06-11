#include "fastrules/async_workflow.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"

#include <algorithm>
#include <thread>
#include <future>
#include <stdexcept>

namespace fastrules {

// ============================================================================
// Thread Pool Implementation
// ============================================================================

AsyncWorkflow::ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                try {
                    task();
                } catch (...) {
                    // Exceptions from tasks are stored in std::promise (via packaged_task).
                    // Unhandled worker exceptions terminate the program — swallow here.
                }
            }
        });
    }
}

AsyncWorkflow::ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            try {
                worker.join();
            } catch (const std::system_error&) {
                // Thread state invalid or already terminated — safe to ignore
            }
        }
    }
}

// ============================================================================
// AsyncWorkflow Implementation
// ============================================================================

AsyncWorkflow::AsyncWorkflow() : threadPool_(std::make_unique<ThreadPool>()) {}

AsyncWorkflow::AsyncWorkflow(Workflow&& workflow) 
    : workflow_(std::move(workflow))
    , threadPool_(std::make_unique<ThreadPool>()) {}

AsyncWorkflow::~AsyncWorkflow() = default;

AsyncWorkflow::AsyncWorkflow(AsyncWorkflow&&) noexcept = default;
AsyncWorkflow& AsyncWorkflow::operator=(AsyncWorkflow&&) noexcept = default;

void AsyncWorkflow::compile(LuaEngine& engine) {
    if (compiled_) return;
    workflow_.compile(engine);
    compiled_ = true;
}

void AsyncWorkflow::waitForCompletion() {
    for (auto& task : pendingTasks_) {
        if (task.valid()) {
            task.wait();
        }
    }
    pendingTasks_.clear();
}

// ============================================================================
// Parallel Execution
// ============================================================================

std::vector<AsyncRuleResult> AsyncWorkflow::executeParallelAsync(
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters) {
    
    if (!compiled_) {
        compile(engine);
    }

    RuleContext context;
    std::vector<AsyncRuleResult> allResults;

    // Build dependency levels
    auto levels = workflow_.resolveExecutionOrder();
    
    // Group by dependency level
    std::vector<std::vector<std::shared_ptr<Rule>>> dependencyLevels;
    {
        std::unordered_map<int, std::shared_ptr<Rule>> remaining;
        for (auto& rule : workflow_.rules) {
            remaining[rule->id] = rule;
        }
        
        while (!remaining.empty()) {
            std::vector<std::shared_ptr<Rule>> level;
            for (auto& [id, rule] : remaining) {
                bool depsSatisfied = true;
                if (rule->dependsOnRuleName.has_value()) {
                    depsSatisfied = true; for (const auto& [remainingId, remainingRule] : remaining) { if (remainingRule->name == rule->dependsOnRuleName.value()) { depsSatisfied = false; break; } }
                }
                if (depsSatisfied) {
                    level.push_back(rule);
                }
            }
            
            for (auto& rule : level) {
                remaining.erase(rule->id);
            }
            if (!level.empty()) {
                dependencyLevels.push_back(std::move(level));
            }
        }
    }

    // Execute each level in parallel
    for (const auto& level : dependencyLevels) {
        std::vector<std::future<AsyncRuleResult>> futures;
        
        for (const auto& rule : level) {
            if (!rule->isActive) continue;
            
            auto engineClone = engine.clone();
            rule->compile(*engineClone);  // Compile rule into the clone
            auto paramsCopy = parameters;
            futures.push_back(
                threadPool_->enqueue([this, eng = std::move(engineClone), params = std::move(paramsCopy), rule, &context]() {
                    AsyncRuleResult asyncResult;
                    try {
                        RuleContext localContext;
                        asyncResult.result = rule->execute(*eng, localContext, params);
                    } catch (...) {
                        asyncResult.exception = std::current_exception();
                    }
                    return asyncResult;
                })
            );
        }
        
        // Collect results from this level
        for (auto& future : futures) {
            auto result = future.get();
            allResults.push_back(result);
            if (result.isSuccess()) {
                context.setResult(result.result.ruleId, "", result.result);
            }
        }
    }

    return allResults;
}

// ============================================================================
// C++20 Coroutine Functions
// ============================================================================

AsyncRulePromise coExecuteRule(std::shared_ptr<Rule> rule,
                               LuaEngine& engine,
                               RuleContext& context,
                               const std::vector<RuleParameter>& parameters) {
    // This function is intended to be called within a coroutine
    // It wraps rule execution in a coroutine-friendly way
    AsyncRuleResult asyncResult;
    try {
        asyncResult.result = rule->execute(engine, context, parameters);
    } catch (...) {
        asyncResult.exception = std::current_exception();
    }
    co_return asyncResult;
}

// Internal helper for async workflow execution
static std::vector<RuleResult> executeWorkflowLevels(
    Workflow& workflow,
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters,
    AsyncWorkflow::ThreadPool& /*pool*/) {
    
    RuleContext context;
    std::vector<RuleResult> results;

    auto levels = workflow.resolveExecutionOrder();
    
    // Group by dependency level
    std::vector<std::vector<std::shared_ptr<Rule>>> dependencyLevels;
    {
        std::unordered_map<int, std::shared_ptr<Rule>> remaining;
        for (auto& rule : workflow.rules) {
            remaining[rule->id] = rule;
        }
        
        while (!remaining.empty()) {
            std::vector<std::shared_ptr<Rule>> level;
            for (auto& [id, rule] : remaining) {
                bool depsSatisfied = true;
                if (rule->dependsOnRuleName.has_value()) {
                    depsSatisfied = true; for (const auto& [remainingId, remainingRule] : remaining) { if (remainingRule->name == rule->dependsOnRuleName.value()) { depsSatisfied = false; break; } }
                }
                if (depsSatisfied) {
                    level.push_back(rule);
                }
            }
            
            for (auto& rule : level) {
                remaining.erase(rule->id);
            }
            if (!level.empty()) {
                dependencyLevels.push_back(std::move(level));
            }
        }
    }

    // Execute each level - within a level, rules are independent
    for (const auto& level : dependencyLevels) {
        // For sequential execution within a coroutine context
        for (const auto& rule : level) {
            if (!rule->isActive) continue;

            // Check dependency
            if (rule->dependsOnRuleName.has_value()) {
                auto depResult = context.getResult(rule->dependsOnRuleName.value());
                if (!depResult.has_value() || !depResult->isSuccess()) {
                    RuleResult skipResult;
                    skipResult.ruleId = rule->id;
                    skipResult.success = false;
                    skipResult.exception = RuleException("Dependency failed: " + rule->dependsOnRuleName.value());
                    results.push_back(skipResult);
                    continue;
                }
            }

            auto result = rule->execute(engine, context, parameters);
            results.push_back(result);
        }
    }

    return results;
}

AsyncWorkflowTask coExecuteWorkflow(Workflow& workflow,
                                     LuaEngine& engine,
                                     const std::vector<RuleParameter>& parameters) {
    if (!workflow.isCompiled()) {
        workflow.compile(engine);
    }

    auto results = executeWorkflowLevels(workflow, engine, parameters, *std::make_unique<AsyncWorkflow::ThreadPool>(1));
    co_return results;
}

} // namespace fastrules
