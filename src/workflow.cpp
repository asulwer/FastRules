/**
 * @file workflow.cpp
 * @brief Workflow orchestration - rule execution with dependency resolution
 *
 * This file implements the Workflow class for managing rule collections:
 * - Dependency resolution using topological sort (Kahn's algorithm)
 * - Sequential and parallel execution modes
 * - Engine pool management for thread-safe parallel execution
 * - Adaptive execution that chooses sequential vs parallel based on rule count
 *
 * Execution Modes:
 * 1. Sequential (execute): Rules execute one at a time in dependency order
 * 2. Parallel (executeParallel): Independent rules execute concurrently
 * 3. Adaptive (executeAdaptive): Automatically chooses based on threshold
 * 4. Streaming (executeStreaming): Generator-style result streaming
 *
 * Dependency Resolution:
 * - Rules declare dependencies via dependsOnRuleName
 * - Kahn's algorithm produces topological order
 * - Rules at same dependency level can execute in parallel
 *
 * Thread Safety:
 * - Construction: NOT thread-safe
 * - Compilation: NOT thread-safe (compile once)
 * - Sequential execution: Thread-safe with external synchronization
 * - Parallel execution: Thread-safe (uses engine pool)
 *
 * Engine Pool:
 * - Pre-created pool of LuaEngine clones
 * - Each thread acquires an engine, uses it, returns it
 * - Eliminates contention on Lua state during parallel execution
 */

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
#include <chrono>
#include <atomic>
#include <mutex>

// Platform-specific intrinsics for spin-wait
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    #include <intrin.h>  // For _mm_pause on MSVC x86/x64
#endif

namespace fastrules {

Workflow::~Workflow() = default;

void Workflow::validate() {
    auto log = fastrules::logger();
    if (validated_) {
        return;
    }

    std::string workflowName = !name.empty() ? name : (!description.empty() ? description : std::to_string(id));
    log->debug("Validating workflow {}", workflowName);

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

    log->info("Workflow {} validated successfully", workflowName);
    validated_ = true;
}

void Workflow::compile(LuaEngine& engine) {
    auto log = fastrules::logger();
    if (compiled_) {
        return;
    }

    if (!validated_) {
        validate();
    }

    std::string workflowName = !name.empty() ? name : (!description.empty() ? description : std::to_string(id));
    log->debug("Compiling workflow {}", workflowName);

    // Ensure the engine has types and actions bound before compiling
    engine.bindTypesToState();
    engine.bindActionsToState();

    // Auto-discover callbacks from actions before compiling
    // This ensures any callbacks referenced in JSON actions get stub registrations
    auto actions = getAllActions();
    if (!actions.empty()) {
        engine.discoverCallbacks(actions);
    }

    // Compile each rule in the main engine
    for (auto& rule : rules) {
        rule->compile(engine);
    }

    // Pre-create engine clone pool for parallel execution
    // Pool size = number of threads or hardware concurrency, min 2
    size_t poolSize = std::thread::hardware_concurrency();
    if (poolSize < 2) poolSize = 2;
    
    log->debug("Creating engine clone pool with {} clones for workflow {}", poolSize, workflowName);
    
    // Clear and rebuild the engine storage
    enginePoolStorage_.clear();
    enginePoolStorage_.reserve(poolSize);
    
    // Create engine clones and compile rules into them
    for (size_t i = 0; i < poolSize; ++i) {
        auto clone = engine.clone();
        // Pre-compile all rules into the clone
        for (auto& rule : rules) {
            rule->compile(*clone);
        }
        enginePoolStorage_.push_back(std::move(clone));
    }
    
    // Initialize the engine pool with the engines
    // We pass the storage which contains unique_ptrs, but the pool stores raw pointers
    // The pool doesn't own the memory - Workflow owns the engines in enginePoolStorage_
    enginePool_ = std::make_unique<EnginePool>();
    for (const auto& enginePtr : enginePoolStorage_) {
        enginePool_->push(enginePtr.get());
    }
    
    useEnginePool_ = true;

    log->info("Workflow {} compiled successfully ({} engine clones ready, pool initialized)", workflowName, poolSize);
    compiled_ = true;
}

void Workflow::compileParallel(LuaEngine& engine, size_t numThreads) {
    auto log = fastrules::logger();
    std::string workflowName = !name.empty() ? name : (!description.empty() ? description : std::to_string(id));

    if (compiled_) {
        return;
    }

    if (!validated_) {
        validate();
    }

    // Determine number of threads
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;  // Fallback
    }

    // Don't parallelize for small workflows
    if (rules.size() < 10 || numThreads == 1) {
        log->debug("Workflow {} has {} rules, using sequential compilation", workflowName, rules.size());
        compile(engine);
        return;
    }

    log->debug("Compiling workflow {} in parallel with {} threads", workflowName, numThreads);

    // Ensure the engine has types and actions bound before compiling
    engine.bindTypesToState();
    engine.bindActionsToState();

    // Auto-discover callbacks from actions before compiling
    auto actions = getAllActions();
    if (!actions.empty()) {
        engine.discoverCallbacks(actions);
    }

    // Build dependency levels for parallel compilation
    // Rules at the same level can be compiled concurrently
    auto levels = buildCompilationLevels();
    
    log->debug("Workflow {} has {} compilation levels", workflowName, levels.size());

    // Create thread pool for compilation
    std::vector<std::thread> threads;
    std::atomic<size_t> currentLevel{0};
    std::atomic<bool> hasError{false};
    std::exception_ptr compileException;
    std::mutex exceptionMutex;

    // Worker function
    auto worker = [&](size_t workerId) {
        // Each worker gets its own engine clone
        std::unique_ptr<LuaEngine> localEngine;
        try {
            localEngine = engine.clone();
        } catch (...) {
            hasError = true;
            return;
        }

        while (!hasError.load(std::memory_order_acquire)) {
            size_t levelIdx = currentLevel.load(std::memory_order_acquire);
            if (levelIdx >= levels.size()) {
                break;  // All levels done
            }

            // Get rules for this level
            const auto& level = levels[levelIdx];
            
            // Find next uncompiled rule in this level
            Rule* ruleToCompile = nullptr;
            for (auto* rule : level) {
                // Check if rule needs compilation
                bool needsCompile = false;
                {
                    std::lock_guard<std::mutex> lock(exceptionMutex);
                    if (!rule->isCompiled) {
                        needsCompile = true;
                    }
                }
                if (needsCompile) {
                    ruleToCompile = rule;
                    break;
                }
            }

            if (ruleToCompile) {
                try {
                    ruleToCompile->compile(*localEngine);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(exceptionMutex);
                    if (!compileException) {
                        compileException = std::current_exception();
                    }
                    hasError = true;
                    return;
                }
            } else {
                // All rules in this level compiled, move to next level
                size_t expected = levelIdx;
                if (currentLevel.compare_exchange_strong(expected, levelIdx + 1,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                    log->debug("Level {} completed by worker {}", levelIdx, workerId);
                }
                
                // If this was the last level, we're done
                if (levelIdx + 1 >= levels.size()) {
                    break;
                }
            }
        }
    };

    // Launch worker threads
    threads.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker, i);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Check for errors
    if (compileException) {
        std::rethrow_exception(compileException);
    }

    // Verify all rules were compiled
    for (const auto& rule : rules) {
        if (!rule->isCompiled) {
            throw RuleCompilationException("Not all rules were compiled in parallel compilation");
        }
    }

    // Also compile the original engine that was passed in
    // This ensures workflow.execute(engine) works with the same engine
    for (auto& rule : rules) {
        rule->compile(engine);
    }

    // Create engine pool for execution (same as sequential compile)
    size_t poolSize = std::thread::hardware_concurrency();
    if (poolSize < 2) poolSize = 2;
    
    enginePoolStorage_.clear();
    enginePoolStorage_.reserve(poolSize);
    
    for (size_t i = 0; i < poolSize; ++i) {
        auto clone = engine.clone();
        for (auto& rule : rules) {
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

    log->info("Workflow {} compiled in parallel successfully", workflowName);
}

std::vector<std::vector<Rule*>> Workflow::buildCompilationLevels() {
    std::vector<std::vector<Rule*>> levels;
    std::unordered_map<int, Rule*> ruleMap;
    std::unordered_map<int, int> inDegree;

    // Build rule map
    for (auto& rule : rules) {
        ruleMap[rule->id] = rule.get();
        inDegree[rule->id] = 0;
    }

    // Calculate in-degrees based on child rule dependencies
    for (auto& rule : rules) {
        // Each child rule must compile before its parent
        for (const auto& child : rule->childRules) {
            if (child && child->id != 0) {
                inDegree[rule->id]++;
            }
        }
    }

    // Kahn's algorithm for topological sort by levels
    std::vector<Rule*> currentLevel;
    for (auto& rule : rules) {
        if (inDegree[rule->id] == 0) {
            currentLevel.push_back(rule.get());
        }
    }

    while (!currentLevel.empty()) {
        levels.push_back(std::move(currentLevel));
        currentLevel.clear();

        for (auto* rule : levels.back()) {
            // Find parents (rules that have this rule as child)
            for (auto& potentialParent : rules) {
                for (const auto& child : potentialParent->childRules) {
                    if (child.get() == rule) {
                        inDegree[potentialParent->id]--;
                        if (inDegree[potentialParent->id] == 0) {
                            currentLevel.push_back(potentialParent.get());
                        }
                    }
                }
            }
        }
    }

    return levels;
}

bool Workflow::isCompiled() const noexcept {
    return compiled_;
}

std::vector<RuleResult> Workflow::execute(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    auto log = fastrules::logger();
    if (!compiled_) {
        compile(engine);
    }

    std::string workflowName = !name.empty() ? name : (!description.empty() ? description : std::to_string(id));
    log->debug("Executing workflow {}", workflowName);

    RuleContext context;
    std::vector<RuleResult> results;

    auto executionOrder = resolveExecutionOrder();
    log->info("Executing {} rules in workflow {}", executionOrder.size(), workflowName);

    for (auto& rule : executionOrder) {
        // Preference: skip inactive rules entirely - no evaluation, no result
        if (!rule->isActive) {
            continue;
        }

        // Check dependency: if rule depends on another, ensure it succeeded
        if (rule->dependsOnRuleName.has_value()) {
            auto depResult = context.getResult(rule->dependsOnRuleName.value());
            if (!depResult.has_value() || !depResult->isSuccess()) {
                log->debug("Skipping rule {} - dependency failed in workflow {}", rule->name, workflowName);
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

    log->info("Workflow {} executed - {} results", workflowName, results.size());
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
            tracer.record(rule->name, "skip", true, "Rule is inactive");
            continue;
        }

        // Check dependency
        if (rule->dependsOnRuleName.has_value()) {
            ExecutionTraceStep depStep;
            depStep.ruleName = rule->name;
            depStep.stage = "dependency_check";
            depStep.dependencyId = -1;
            depStep.startedAt = std::chrono::steady_clock::now();

            auto depResult = context.getResult(rule->dependsOnRuleName.value());
            depStep.success = depResult.has_value() && depResult->isSuccess();
            depStep.endedAt = std::chrono::steady_clock::now();

            if (!depResult.has_value() || !depResult->isSuccess()) {
                depStep.message = "Dependency '" + rule->dependsOnRuleName.value() + "' failed or not found";
                tracer.addStep(std::move(depStep));
                continue;
            }
            depStep.message = "Dependency '" + rule->dependsOnRuleName.value() + "' satisfied";
            tracer.addStep(std::move(depStep));
        }

        // Execute rule with timing
        ExecutionTraceStep execStep;
        execStep.ruleName = rule->name;
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

        context.setResult(rule->name, result);

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

    // Execute each level in parallel using pre-compiled engine clones from the pool
    for (const auto& level : dependencyLevels) {
        std::vector<std::future<std::pair<int, RuleResult>>> futures;

        for (const auto& rule : level) {
            if (!rule->isActive) {
                continue;
            }

            futures.push_back(
                std::async(std::launch::async, [&context, &parameters, rule, this]() {
                    // Acquire a pre-compiled engine clone from the pool
                    auto* threadEngine = acquireEngine();
                    
                    // Safety check: if pool exhausted, return error
                    if (!threadEngine) {
                        RuleResult errorResult;
                        errorResult.ruleName = rule->name;
                        errorResult.success = false;
                        errorResult.exception = RuleException("Failed to acquire engine from pool - timeout or pool empty");
                        return std::make_pair(rule->id, errorResult);
                    }
                    
                    // Check dependency before execution
                    if (rule->dependsOnRuleName.has_value()) {
                        auto depResult = context.getResult(rule->dependsOnRuleName.value());
                        if (!depResult.has_value() || !depResult->isSuccess()) {
                            RuleResult skipResult;
                            skipResult.ruleName = rule->name;
                            skipResult.success = false;
                            skipResult.exception = RuleException("Dependency failed: " + rule->dependsOnRuleName.value());
                            releaseEngine(threadEngine);
                            return std::make_pair(rule->id, skipResult);
                        }
                    }

                    // Execute with the cloned engine (no mutex contention!)
                    auto result = rule->execute(*threadEngine, context, parameters);
                    
                    // Release engine back to the pool
                    releaseEngine(threadEngine);
                    
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
            context.setResult(result.ruleName, result);
        }
    }

    return results;
}

LuaEngine* Workflow::acquireEngine() {
    // Use engine pool when available
    if (useEnginePool_ && enginePool_) {
        // Fast path: try immediate pop
        LuaEngine* engine = enginePool_->pop();
        if (engine) {
            return engine;
        }
        
        // Slow path: wait with timeout
        engine = enginePool_->tryPop(std::chrono::milliseconds(100));
        if (engine) {
            return engine;
        }
        
        // Timeout - log warning and return nullptr
        auto log = fastrules::logger();
        if (log) {
            log->warn("Timeout waiting for engine from pool (pool size: {})", 
                       enginePoolStorage_.size());
        }
        return nullptr;
    }
    
    // Legacy fallback: should not reach here if compiled properly
    return nullptr;
}

void Workflow::releaseEngine(LuaEngine* engine) {
    if (!engine) return;
    
    if (useEnginePool_ && enginePool_) {
        // Push back to engine pool
        enginePool_->push(engine);
    }
}

std::future<std::vector<RuleResult>> Workflow::executeAsync(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    // Return future that will execute asynchronously
    // Capture parameters by VALUE to avoid dangling reference when async runs later
    return std::async(std::launch::async, [this, &engine, parameters]() {
        return this->execute(engine, parameters);
    });
}

std::vector<RuleResult> Workflow::executeAdaptive(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    if (!compiled_) {
        compile(engine);
    }
    
    // Auto-detection mode: occasionally test both strategies and update threshold
    if (autoDetectThreshold_ && rules.size() > 2) {
        // Every 100 executions, test if threshold should change
        static thread_local size_t checkCounter = 0;
        if (++checkCounter % 100 == 0) {
            // Test sequential execution time
            auto start = std::chrono::high_resolution_clock::now();
            auto seqResults = execute(engine, parameters);
            auto seqEnd = std::chrono::high_resolution_clock::now();
            double seqTime = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - start).count());
            
            // Update rolling average for sequential
            sequentialAvgTime_ = (sequentialAvgTime_ * sequentialRuns_ + seqTime) / (sequentialRuns_ + 1);
            sequentialRuns_++;
            
            // Test parallel execution time
            start = std::chrono::high_resolution_clock::now();
            auto parResults = executeParallel(engine, parameters);
            auto parEnd = std::chrono::high_resolution_clock::now();
            double parTime = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(parEnd - start).count());
            
            // Update rolling average for parallel
            parallelAvgTime_ = (parallelAvgTime_ * parallelRuns_ + parTime) / (parallelRuns_ + 1);
            parallelRuns_++;
            
            // Adjust threshold based on which is faster
            if (sequentialAvgTime_ < parallelAvgTime_ * 0.8) {
                // Sequential is significantly faster, increase threshold
                adaptiveThreshold_ = std::min(adaptiveThreshold_ + 1, size_t(20));
            } else if (parallelAvgTime_ < sequentialAvgTime_ * 0.8) {
                // Parallel is significantly faster, decrease threshold
                adaptiveThreshold_ = std::max(adaptiveThreshold_ - 1, size_t(2));
            }
            
            return seqTime < parTime ? seqResults : parResults;
        }
    }
    
    // Normal execution based on current threshold
    if (rules.size() <= adaptiveThreshold_) {
        return execute(engine, parameters);        // Sequential - avoid thread overhead
    } else {
        return executeParallel(engine, parameters);  // Parallel - maximize concurrency
    }
}

StreamingResult Workflow::executeStreaming(LuaEngine& engine, const std::vector<RuleParameter>& parameters) {
    if (!compiled_) {
        compile(engine);
    }

    // Capture workflow state by value for the generator.
    // The generator owns its own context and index so multiple StreamingResult
    // instances (and instances moved across threads) do not share state.
    auto rulesCopy = rules;

    return StreamingResult([this, &engine, parameters, rulesCopy,
                            ctx = std::make_shared<RuleContext>(),
                            idx = std::make_shared<size_t>(0)]() mutable -> std::optional<RuleResult> {
        // Find next rule to execute
        while (*idx < rulesCopy.size()) {
            auto& rule = rulesCopy[(*idx)++];

            if (!rule->isActive) {
                continue;
            }

            // Check dependency
            if (rule->dependsOnRuleName.has_value()) {
                auto depResult = ctx->getResult(rule->dependsOnRuleName.value().c_str());
                if (!depResult.has_value() || !depResult->isSuccess()) {
                    continue;
                }
            }

            auto result = rule->execute(engine, *ctx, parameters);
            ctx->setResult(rule->name, result);

            if (!result.skipped) {
                return result;
            }
        }

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
    std::unordered_map<int, std::shared_ptr<Rule>> ruleMap;
    std::unordered_map<int, int> inDegree;

    for (const auto& rule : rules) {
        ruleMap[rule->id] = rule;
        inDegree[rule->id] = 0;
    }

    // Count dependencies
    for (const auto& rule : rules) {
        if (rule->dependsOnRuleName.has_value()) {
            // Look up the rule by name in the map
            for (const auto& [lookupId, r] : ruleMap) {
                if (r->name == rule->dependsOnRuleName.value().c_str()) {
                    inDegree[rule->id]++;
                    break;
                }
            }
        }
    }

    std::vector<std::vector<std::shared_ptr<Rule>>> levels;

    while (!ruleMap.empty()) {
        std::vector<std::shared_ptr<Rule>> level;

        // Find all rules with in-degree 0
        for (const auto& [ruleId, rule] : ruleMap) {
            if (inDegree[ruleId] == 0) {
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
            for (const auto& [remainingId, r] : ruleMap) {
                (void)remainingId;
                if (r->dependsOnRuleName.has_value() && r->dependsOnRuleName.value() == rule->name) {
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
    std::unordered_map<int, std::shared_ptr<Rule>> ruleMap;
    for (const auto& rule : rules) {
        ruleMap[rule->id] = rule;
    }

    enum class VisitState { Unvisited, Visiting, Visited };
    std::unordered_map<int, VisitState> state;

    for (const auto& [ruleId, _] : ruleMap) {
        state[ruleId] = VisitState::Unvisited;
    }

    std::function<bool(int)> dfs = [&](int ruleId) -> bool {
        state[ruleId] = VisitState::Visiting;

        auto it = ruleMap.find(ruleId);
        if (it != ruleMap.end() && it->second->dependsOnRuleName.has_value()) {
            // Look up the dependency ID from the name
            int depId = -1;
            for (const auto& [id, r] : ruleMap) {
                if (r->name == it->second->dependsOnRuleName.value()) {
                    depId = id;
                    break;
                }
            }
            if (depId != -1) {
                if (state[depId] == VisitState::Visiting) {
                    return true; // Cycle found
                }
                if (state[depId] == VisitState::Unvisited && dfs(depId)) {
                    return true;
                }
            }
        }

        state[ruleId] = VisitState::Visited;
        return false;
    };

    for (const auto& [ruleId, _] : ruleMap) {
        if (state[ruleId] == VisitState::Unvisited && dfs(ruleId)) {
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

Workflow::Builder::Builder(int workflowId)
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

// ============================================================================
// Dependency Graph Visualization
// ============================================================================

std::string Workflow::toGraph() const {
    std::ostringstream oss;
    
    // DOT graph header
    oss << "digraph Workflow {\n";
    oss << "  rankdir=TB;\n";
    oss << "  node [shape=box, style=filled, fillcolor=lightblue];\n";
    oss << "  edge [arrowhead=vee];\n\n";
    
    // Graph title
    oss << "  label=\"Workflow " << id;
    if (!description.empty()) {
        oss << "\\n" << description;
    }
    oss << "\";\n";
    oss << "  labelloc=t;\n\n";
    
    // Collect all rules (including nested children)
    std::function<void(const std::vector<std::shared_ptr<Rule>>&)> collectRules = 
        [&](const std::vector<std::shared_ptr<Rule>>& ruleList) {
        for (const auto& rule : ruleList) {
            // Add rule node
            oss << "  \"rule_" << rule->id << "\" [label=\"Rule " << rule->id;
            if (!rule->description.empty()) {
                oss << "\\n" << rule->description;
            }
            // Show expression snippet
            if (!rule->expression.empty()) {
                std::string expr = rule->expression;
                if (expr.length() > 30) expr = expr.substr(0, 27) + "...";
                std::string newline = "\\n";
                std::string quote = "\"";
                oss << newline << "[" << quote << expr << " " << quote << "]";
            }
            // Color inactive rules differently
            if (!rule->isActive) {
                oss << "\", fillcolor=gray, fontcolor=white";
            }
            oss << "\"];\n";
            
            // Add dependency edges
            if (rule->dependsOnRuleName.has_value()) {
                // Find parent ID by name
                int parentId = -1;
                for (const auto& r : rules) {
                    if (r->name == rule->dependsOnRuleName.value()) {
                        parentId = r->id;
                        break;
                    }
                }
                if (parentId != -1) {
                    oss << "  \"rule_" << parentId << "\" -> \"rule_" << rule->id << "\"";
                    oss << " [color=red, penwidth=2.0, label=\"depends on\"];\n";
                }
            }
            
            // Recurse into child rules
            if (!rule->childRules.empty()) {
                oss << "  \"rule_" << rule->id << "\" -> {\n";
                for (const auto& child : rule->childRules) {
                    oss << "    \"rule_" << child->id << "\"\n";
                }
                oss << "  } [style=dashed, label=\"parent/child\"];\n";
                collectRules(rule->childRules);
            }
        }
    };
    
    collectRules(rules);
    
    // Footer
    oss << "}\n";
    return oss.str();
}

} // namespace fastrules
