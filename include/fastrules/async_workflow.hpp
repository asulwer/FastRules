/**
 * @file async_workflow.hpp
 * @brief Async workflow execution with thread pool
 * 
 * AsyncWorkflow provides advanced execution capabilities:
 * - Thread pool for parallel rule execution
 * - Async/await support for coroutines
 * - Promise-based async results
 * - Engine pool management
 * 
 * Relationship to Workflow:
 * - Workflow is the basic orchestrator
 * - AsyncWorkflow adds thread pool and async capabilities
 * - AsyncWorkflow wraps a Workflow and adds execution infrastructure
 * 
 * Thread Pool:
 * Uses a fixed-size thread pool (configurable at construction).
 * Tasks are enqueued and executed by worker threads.
 * Thread pool is automatically shut down on destruction.
 * 
 * Engine Pool:
 * Maintains a pool of cloned LuaEngines for parallel execution.
 * Engines are acquired before execution and released after.
 * 
 * Execution Model:
 * - Build dependency levels (rules that can execute in parallel)
 * - For each level, enqueue tasks to thread pool
 * - Wait for all tasks in level to complete
 * - Move to next level
 * 
 * Example:
 * @code
 * // Create async workflow
 * AsyncWorkflow async(std::move(workflow), 4); // 4 threads
 * 
 * // Compile
 * async.compile(engine);
 * 
 * // Execute in parallel
 * auto results = async.executeParallelAsync(engine, params);
 * 
 * // Wait for all pending tasks
 * async.waitForCompletion();
 * @endcode
 */

#pragma once

#include "fastrules/workflow.hpp"
#include "fastrules/engine_pool.hpp"

#include <future>
#include <memory>
#include <vector>
#include <coroutine>

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;
struct AsyncRuleResult;

/**
 * @brief Async-capable workflow wrapper
 * 
 * Extends Workflow with:
 * - Thread pool for parallel execution
 * - Async/await support
 * - Engine pool for thread-safe Lua execution
 * 
 * Thread Safety:
 * - Construction: NOT thread-safe
 * - Compilation: NOT thread-safe
 * - Execution: Thread-safe (uses internal synchronization)
 */
class AsyncWorkflow {
public:
    // ========================================================================
    // Construction / Destruction
    // ========================================================================
    
    /**
     * @brief Construct with thread count
     * 
     * Creates an empty async workflow with the specified thread pool size.
     * 
     * @param threadCount Number of threads in the pool (default: hardware concurrency)
     */
    explicit AsyncWorkflow(size_t threadCount = std::thread::hardware_concurrency());

    /**
     * @brief Construct from existing workflow
     * 
     * Takes ownership of the workflow and adds async capabilities.
     * 
     * @param workflow The workflow to wrap
     * @param threadCount Number of threads in the pool
     */
    AsyncWorkflow(Workflow&& workflow, size_t threadCount = std::thread::hardware_concurrency());

    /// @brief Destructor - shuts down thread pool
    ~AsyncWorkflow();

    /// @brief Move constructor
    AsyncWorkflow(AsyncWorkflow&&) noexcept;
    
    /// @brief Move assignment
    AsyncWorkflow& operator=(AsyncWorkflow&&) noexcept;
    
    /// @brief Disable copy
    AsyncWorkflow(const AsyncWorkflow&) = delete;
    
    /// @brief Disable copy assignment
    AsyncWorkflow& operator=(const AsyncWorkflow&) = delete;

    // ========================================================================
    // Accessors
    // ========================================================================
    
    /// @brief Get the wrapped workflow
    [[nodiscard]] Workflow& workflow() noexcept { return workflow_; }
    
    /// @brief Get the wrapped workflow (const)
    [[nodiscard]] const Workflow& workflow() const noexcept { return workflow_; }

    /// @brief Check if compiled
    [[nodiscard]] bool isCompiled() const noexcept { return compiled_; }

    // ========================================================================
    // Compilation
    // ========================================================================
    
    /**
     * @brief Compile the workflow for async execution
     * 
     * Compiles all rules and creates the engine pool.
     * 
     * @param engine The master LuaEngine
     */
    void compile(LuaEngine& engine);

    // ========================================================================
    // Execution - Async
    // ========================================================================
    
    /**
     * @brief Execute rules in parallel asynchronously
     * 
     * Builds dependency levels and executes each level in parallel
     * using the thread pool.
     * 
     * @param engine The master LuaEngine
     * @param parameters Parameters to pass to rules
     * @return Vector of async results (may contain exceptions)
     * 
     * Thread Safety: Thread-safe. Can be called from any thread.
     */
    std::vector<AsyncRuleResult> executeParallelAsync(
        LuaEngine& engine,
        const std::vector<RuleParameter>& parameters);

    /**
     * @brief Wait for all pending tasks to complete
     * 
     * Blocks until all async tasks finish.
     */
    void waitForCompletion();

private:
    // ========================================================================
    // State
    // ========================================================================
    
    Workflow workflow_;                      ///< The wrapped workflow
    size_t threadCount_;                     ///< Number of threads
    bool compiled_ = false;                  ///< Whether compiled

    // ========================================================================
    // Thread Pool (PIMPL)
    // ========================================================================
    
    struct ThreadPoolImpl;
    std::unique_ptr<ThreadPoolImpl> threadPool_;  ///< Thread pool

    // ========================================================================
    // Engine Pool
    // ========================================================================
    
    bool useEnginePool_ = false;                                    ///< Whether using pool
    std::unique_ptr<EnginePool> enginePool_;                      ///< Engine pool
    std::vector<std::unique_ptr<LuaEngine>> enginePoolStorage_;    ///< Engine storage
    std::vector<std::future<void>> pendingTasks_;                  ///< Pending tasks

    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Acquire an engine from the pool
     * 
     * @return Pointer to engine, or nullptr on timeout
     */
    LuaEngine* acquireEngine();

    /**
     * @brief Return an engine to the pool
     * 
     * @param engine The engine to return
     */
    void releaseEngine(LuaEngine* engine);
};

/**
 * @brief Coroutine-based rule execution
 * 
 * Executes a rule as a coroutine, capturing any exceptions.
 * 
 * @param rule The rule to execute
 * @param engine The LuaEngine
 * @param context The execution context
 * @param parameters The parameters
 * @return Async result
 */
AsyncRulePromise coExecuteRule(std::shared_ptr<Rule> rule,
                               LuaEngine& engine,
                               RuleContext& context,
                               const std::vector<RuleParameter>& parameters);

/**
 * @brief Coroutine-based workflow execution
 * 
 * Executes an entire workflow asynchronously.
 * 
 * @param workflow The workflow to execute
 * @param engine The LuaEngine
 * @param parameters The parameters
 * @param threadCount Number of threads
 * @return Async task yielding results
 */
AsyncWorkflowTask coExecuteWorkflow(Workflow& workflow,
                                     LuaEngine& engine,
                                     const std::vector<RuleParameter>& parameters,
                                     size_t threadCount);

} // namespace fastrules
