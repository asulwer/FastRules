/**
 * @file async_workflow.cpp
 * @brief Async workflow execution with coroutines and thread pools
 * 
 * This file implements AsyncWorkflow for non-blocking rule execution:
 * - Thread pool for parallel execution
 * - Coroutine-based async tasks (C++20 co_await/co_return)
 * - Engine pool management for thread-safe execution
 * - Dependency-level parallel execution
 * 
 * Thread Pool:
 * - Fixed-size pool of worker threads
 * - Task queue with condition variable notification
 * - Graceful shutdown with stop flag
 * - Exception-safe task execution
 * 
 * Execution Model:
 * - Rules at same dependency level execute in parallel
 * - Each rule acquires an engine from the pool
 * - Results are gathered as futures complete
 * - Supports both eager (parallel) and lazy (coroutine) execution
 * 
 * Coroutines:
 * - coExecuteRule: Async single rule execution
 * - coExecuteWorkflow: Async workflow with co_await
 * - Returns AsyncRulePromise/AsyncWorkflowTask
 * 
 * Thread Safety:
 * - Thread pool: Safe for concurrent enqueue
 * - Engine pool: Safe for acquire/release
 * - Results: Gathered sequentially after parallel execution
 * 
 * Performance:
 * - Best for I/O-bound or long-running rules
 * - Overhead from thread synchronization
 * - Engine cloning is expensive (done once at compile)
 */

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
#include <coroutine>
#include <optional>

namespace fastrules {

// ============================================================================
// Thread Pool Implementation
// ============================================================================

/**
 * @brief Internal thread pool for async execution
 * 
 * Implements a classic producer-consumer pattern:
 * - Producers: User code enqueueing tasks
 * - Consumers: Worker threads waiting on condition variable
 * - Shutdown: Stop flag signals workers to exit
 * 
 * Task Queue:
 * - std::function<void()> for type erasure
 * - std::packaged_task for future-based results
 * - Mutex-protected with condition variable notification
 */
struct AsyncWorkflow::ThreadPoolImpl {
    std::vector<std::thread> workers_;           /// Worker threads
    std::queue<std::function<void()>> tasks_;  /// Task queue
    std::mutex queueMutex_;                      /// Protects task queue
    std::condition_variable condition_;        /// Notifies workers
    bool stop_ = false;                          /// Shutdown signal
    
    /**
     * @brief Construct thread pool with numThreads workers
     * 
     * Creates worker threads that loop:
     * 1. Wait for task or stop signal
     * 2. Pop task from queue
     * 3. Execute task (with exception catch)
     * 4. Loop back to wait
     */
    explicit ThreadPoolImpl(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    try { task(); } catch (...) {}
                }
            });
        }
    }
    
    /**
     * @brief Destructor - graceful shutdown
     * 
     * Sets stop flag, notifies all workers, joins threads.
     * Any remaining tasks are discarded.
     */
    ~ThreadPoolImpl() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                try { worker.join(); } catch (...) {}
            }
        }
    }
    
    /**
     * @brief Enqueue a task and return a future for its result
     * 
     * Wraps the function in a packaged_task for future-based result.
     * Throws if called on stopped pool.
     * 
     * @param func Callable to execute
     * @param args Arguments to pass to func
     * @return Future containing the result
     */
    template<typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        using return_type = std::invoke_result_t<Func, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
};

// ============================================================================
// AsyncWorkflow Implementation
// ============================================================================

/**
 * @brief Construct empty AsyncWorkflow
 * @param threadCount Number of threads in the pool
 */
AsyncWorkflow::AsyncWorkflow(size_t threadCount) 
    : threadCount_(threadCount)
    , threadPool_(std::make_unique<ThreadPoolImpl>(threadCount)) {}

/**
 * @brief Construct from existing Workflow (move)
 * @param workflow Workflow to move from
 * @param threadCount Number of threads in the pool
 */
AsyncWorkflow::AsyncWorkflow(Workflow&& workflow, size_t threadCount) 
    : workflow_(std::move(workflow))
    , threadCount_(threadCount)
    , threadPool_(std::make_unique<ThreadPoolImpl>(threadCount)) {}

/**
 * @brief Destructor - thread pool cleans up automatically
 */
AsyncWorkflow::~AsyncWorkflow() = default;

// Move operations (default is fine since unique_ptr handles cleanup)
AsyncWorkflow::AsyncWorkflow(AsyncWorkflow&&) noexcept = default;
AsyncWorkflow& AsyncWorkflow::operator=(AsyncWorkflow&&) noexcept = default;

/**
 * @brief Compile workflow for async execution
 * 
 * Compiles underlying workflow and creates engine pool:
 * 1. Compile workflow in base engine
 * 2. Clone engine for each thread
 * 3. Compile rules into each clone
 * 4. Initialize engine pool
 * 
 * Note: Lua state doesn't clone well, so each engine
 * is compiled individually rather than cloning compiled state.
 */
void AsyncWorkflow::compile(LuaEngine& engine) {
    if (compiled_) return;
    
    workflow_.compile(engine);
    
    size_t poolSize = threadCount_;
    if (poolSize < 2) poolSize = 2;
    
    enginePoolStorage_.clear();
    enginePoolStorage_.reserve(poolSize);
    
    for (size_t i = 0; i < poolSize; ++i) {
        auto clone = engine.clone();
        for (auto& rule : workflow_.rules) {
            rule->compile(*clone);
        }
        enginePoolStorage_.push_back(std::move(clone));
    }
    
    enginePool_ = std::make_unique<EnginePool>();
    for (const auto& enginePtr : enginePoolStorage_) {
        enginePool_>&gt;push(enginePtr.get());
    }
    
    useEnginePool_ = true;
    compiled_ = true;
}

/**
 * @brief Acquire an engine from the pool
 * @return Engine pointer or nullptr if unavailable
 */
LuaEngine* AsyncWorkflow::acquireEngine() {
    if (useEnginePool_ && enginePool_) {
        if (LuaEngine* eng = enginePool_>&gt;pop()) return eng;
        return enginePool_>&gt;tryPop(std::chrono::milliseconds(100));
    }
    return nullptr;
}

/**
 * @brief Return an engine to the pool
 * @param engine Engine to release
 */
void AsyncWorkflow::releaseEngine(LuaEngine* engine) {
    if (!engine) return;
    if (useEnginePool_ && enginePool_) enginePool_>&gt;push(engine);
}

/**
 * @brief Wait for all pending async tasks to complete
 */
void AsyncWorkflow::waitForCompletion() {
    for (auto& task : pendingTasks_) {
        if (task.valid()) task.wait();
    }
    pendingTasks_.clear();
}

/**
 * @brief Execute rules in parallel by dependency level
 * 
 * Rules at same dependency level execute concurrently.
 * Rules at different levels wait for previous level.
 * 
 * @param engine Base engine (for compilation if needed)
 * @param parameters Rule parameters
 * @return Vector of async results in rule order
 */
std::vector<AsyncRuleResult> AsyncWorkflow::executeParallelAsync(
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters) {
    
    if (!compiled_) compile(engine);

    std::vector<AsyncRuleResult> allResults;

    // Build dependency levels using topological sort
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

    // Execute each dependency level in parallel
    for (const auto& level : dependencyLevels) {
        std::vector<std::future<AsyncRuleResult>> futures;
        futures.reserve(level.size());
        
        for (const auto& rule : level) {
            // Skip inactive rules
            if (!rule->isActive) {
                AsyncRuleResult skip;
                skip.result.ruleName = rule->name;
                skip.result.skipped = true;
                skip.result.success = true;
                allResults.push_back(std::move(skip));
                continue;
            }
            
            // Enqueue rule execution to thread pool
            futures.push_back(
                threadPool_>&gt;enqueue([this, &parameters, rule]() -> AsyncRuleResult {
                    AsyncRuleResult asyncResult;
                    asyncResult.result.ruleName = rule->name;
                    
                    LuaEngine* eng = acquireEngine();
                    if (!eng) {
                        asyncResult.result.success = false;
                        asyncResult.result.exception = RuleException("Failed to acquire engine from pool");
                        return asyncResult;
                    }
                    
                    RuleContext localContext;
                    try {
                        asyncResult.result = rule->execute(*eng, localContext, parameters);
                    } catch (...) {
                        asyncResult.result.success = false;
                        asyncResult.exception = std::current_exception();
                    }
                    
                    releaseEngine(eng);
                    return asyncResult;
                })
            );
        }
        
        // Gather results for this level before proceeding
        for (auto& future : futures) {
            allResults.push_back(std::move(future.get()));
        }
    }

    return allResults;
}

/**
 * @brief Coroutine: Execute a single rule asynchronously
 * 
 * Usage:
 * @code
 * auto task = coExecuteRule(rule, engine, context, params);
 * auto result = co_await task;
 * @endcode
 */
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

/**
 * @brief Coroutine: Execute entire workflow asynchronously
 * 
 * Usage:
 * @code
 * auto task = coExecuteWorkflow(workflow, engine, params, 4);
 * auto results = co_await task;
 * @endcode
 */
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
