#include "fastrules/async_workflow.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"

#include <algorithm>
#include <thread>
#include <future>
#include <stdexcept>
#include <queue>
#include <atomic>
#include <optional>
#include <memory>

namespace fastrules {

// ============================================================================
// Thread Pool Implementation
// ============================================================================

struct AsyncWorkflow::ThreadPoolImpl {
    std::unique_ptr<WorkStealingThreadPool> workStealingPool_;
    
    explicit ThreadPoolImpl(size_t numThreads) {
        workStealingPool_ = std::make_unique<WorkStealingThreadPool>(numThreads);
    }
    
    ~ThreadPoolImpl() = default;
    
    template<typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        return workStealingPool_->enqueue(std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

// ============================================================================
// AsyncWorkflow Implementation
// ============================================================================

AsyncWorkflow::AsyncWorkflow(size_t threadCount) 
    : threadCount_(threadCount)
    , threadPool_(std::make_unique<ThreadPoolImpl>(threadCount)) {}

AsyncWorkflow::AsyncWorkflow(Workflow&& workflow, size_t threadCount) 
    : workflow_(std::move(workflow))
    , threadCount_(threadCount)
    , threadPool_(std::make_unique<ThreadPoolImpl>(threadCount)) {}

AsyncWorkflow::~AsyncWorkflow() = default;

AsyncWorkflow::AsyncWorkflow(AsyncWorkflow&&) noexcept = default;
AsyncWorkflow& AsyncWorkflow::operator=(AsyncWorkflow&&) noexcept = default;

void AsyncWorkflow::compile(LuaEngine& engine) {
    if (compiled_) return;
    
    // Compile the underlying workflow
    workflow_.compile(engine);
    
    // Set up AsyncWorkflow's own engine pool
    size_t poolSize = threadCount_;
    if (poolSize < 2) poolSize = 2;
    
    enginePoolStorage_.clear();
    enginePoolStorage_.reserve(poolSize);
    
    // Compile rules into each clone (not batch compiled - Lua state doesn't clone well)
    for (size_t i = 0; i < poolSize; ++i) {
        auto clone = engine.clone();
        for (auto& rule : workflow_.rules) {
            rule->compile(*clone);
        }
        enginePoolStorage_.push_back(std::move(clone));
    }
    
    enginePool_ = std::make_unique<EnginePool>();
    for (const auto& enginePtr : enginePoolStorage_) {
        enginePool_->push(enginePtr.get());
    }
    
    useEnginePool_ = true;
    compiled_ = true;
}

LuaEngine* AsyncWorkflow::acquireEngine() {
    if (useEnginePool_ && enginePool_) {
        if (LuaEngine* eng = enginePool_->pop()) return eng;
        return enginePool_->tryPop(std::chrono::milliseconds(100));
    }
    return nullptr;
}

void AsyncWorkflow::releaseEngine(LuaEngine* engine) {
    if (!engine) return;
    if (useEnginePool_ && enginePool_) enginePool_->push(engine);
}

void AsyncWorkflow::waitForCompletion() {
    for (auto& task : pendingTasks_) {
        if (task.valid()) task.wait();
    }
    pendingTasks_.clear();
}

std::vector<AsyncRuleResult> AsyncWorkflow::executeParallelAsync(
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters) {
    
    if (!compiled_) compile(engine);

    std::vector<AsyncRuleResult> allResults;

    // Build dependency levels
    std::vector<std::vector<std::shared_ptr<Rule>>> dependencyLevels;
    {
        std::unordered_map<int, std::shared_ptr<Rule>> remaining;
        for (auto& rule : workflow_.rules) remaining[rule->id] = rule;
        
        while (!remaining.empty()) {
            std::vector<std::shared_ptr<Rule>> level;
            for (auto& [id, rule] : remaining) {
                bool depsSatisfied = true;
                if (rule->dependsOnRuleName.has_value()) {
                    for (const auto& [rid, rrule] : remaining) {
                        if (rrule->name == rule->dependsOnRuleName.value()) {
                            depsSatisfied = false; break;
                        }
                    }
                }
                if (depsSatisfied) level.push_back(rule);
            }
            for (auto& rule : level) remaining.erase(rule->id);
            if (!level.empty()) dependencyLevels.push_back(std::move(level));
        }
    }

    // Shared context across dependency levels so rules can read results from
    // prior levels via context.getResult(...).
    RuleContext sharedContext;

    for (const auto& level : dependencyLevels) {
        std::vector<std::future<AsyncRuleResult>> futures;
        futures.reserve(level.size());
        std::vector<std::shared_ptr<RuleContext>> localContexts;
        localContexts.reserve(level.size());
        
        for (const auto& rule : level) {
            if (!rule->isActive) {
                AsyncRuleResult skip;
                skip.result.ruleName = rule->name;
                skip.result.skipped = true;
                skip.result.success = true;  // Skipped rules count as success
                allResults.push_back(std::move(skip));
                continue;
            }
            
            // Give each rule a copy of the shared context for reading prior
            // level results, while isolating its own writes.
            auto localCtx = std::make_shared<RuleContext>(sharedContext);
            localContexts.push_back(localCtx);

            futures.push_back(
                threadPool_->enqueue([this, parameters, rule, localCtx]() -> AsyncRuleResult {
                    AsyncRuleResult asyncResult;
                    asyncResult.result.ruleName = rule->name;
                    
                    LuaEngine* eng = acquireEngine();
                    if (!eng) {
                        asyncResult.result.success = false;
                        asyncResult.result.exception = RuleException("Failed to acquire engine from pool");
                        return asyncResult;
                    }
                    
                    try {
                        asyncResult.result = rule->execute(*eng, *localCtx, parameters);
                    } catch (...) {
                        asyncResult.result.success = false;
                        asyncResult.exception = std::current_exception();
                    }
                    
                    releaseEngine(eng);
                    return asyncResult;
                })
            );
        }
        
        for (auto& future : futures) {
            allResults.push_back(std::move(future.get()));
            // Merge this rule's result into the shared context so the next
            // dependency level can read it.
            const auto& latest = allResults.back().result;
            sharedContext.setResult(latest.ruleName, latest);
        }
    }

    return allResults;
}

AsyncRulePromise coExecuteRule(std::shared_ptr<Rule> rule,
                               LuaEngine& engine,
                               RuleContext& context,
                               const std::vector<RuleParameter>& parameters) {
    AsyncRuleResult asyncResult;
    try {
        asyncResult.result = rule->execute(engine, context, parameters);
    } catch (...) {
        asyncResult.exception = std::current_exception();
    }
    co_return asyncResult;
}

AsyncWorkflowTask coExecuteWorkflow(Workflow& workflow,
                                     LuaEngine& engine,
                                     const std::vector<RuleParameter>& parameters,
                                     size_t threadCount) {
    AsyncWorkflow async(std::move(workflow), threadCount);
    async.compile(engine);
    
    auto asyncResults = async.executeParallelAsync(engine, parameters);
    
    std::vector<RuleResult> results;
    results.reserve(asyncResults.size());
    
    for (auto& ar : asyncResults) {
        if (ar.exception) std::rethrow_exception(ar.exception);
        results.push_back(std::move(ar.result));
    }
    
    co_return results;
}

} // namespace fastrules
