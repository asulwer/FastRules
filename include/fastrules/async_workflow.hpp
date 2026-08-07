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
#include "fastrules/work_stealing_thread_pool.hpp"

#include <future>
#include <memory>
#include <vector>
#include <coroutine>
#include <exception>

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;

// AsyncWorkflowTask - coroutine return type for workflow execution
class AsyncWorkflowTask {
public:
    struct promise_type {
        std::vector<RuleResult> results;
        std::exception_ptr exception;
        
        auto get_return_object() {
            return AsyncWorkflowTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(std::vector<RuleResult> value) {
            results = std::move(value);
        }
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    AsyncWorkflowTask() = default;
    explicit AsyncWorkflowTask(handle_type h) : handle_(h) {}
    ~AsyncWorkflowTask() { if (handle_) handle_.destroy(); }
    
    AsyncWorkflowTask(const AsyncWorkflowTask&) = delete;
    AsyncWorkflowTask& operator=(const AsyncWorkflowTask&) = delete;
    AsyncWorkflowTask(AsyncWorkflowTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    AsyncWorkflowTask& operator=(AsyncWorkflowTask&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    [[nodiscard]] std::vector<RuleResult> get() {
        if (handle_.done()) return handle_.promise().results;
        handle_.resume();
        if (handle_.promise().exception) std::rethrow_exception(handle_.promise().exception);
        return handle_.promise().results;
    }

private:
    handle_type handle_;
};

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
    /// @warning Only safe while no task is in flight. Enqueued tasks capture
    /// the AsyncWorkflow address to reach the engine pool, so moving one that
    /// still has running work leaves those tasks pointing at the moved-from
    /// object. executeParallelAsync() waits for all of its own tasks before
    /// returning, so this only matters if you move concurrently with it.
    AsyncWorkflow(AsyncWorkflow&&) noexcept;

    /// @brief Move assignment
    /// @warning See the move constructor.
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
    
    // DECLARATION ORDER IS LOAD-BEARING.
    //
    // Members are destroyed in reverse declaration order, so the thread pool
    // must be declared LAST: its destructor joins the worker threads, and any
    // task still running holds a LuaEngine borrowed from enginePoolStorage_.
    // With the pool declared first, the engines were freed while workers were
    // still using them - a use-after-free on shutdown.

    Workflow workflow_;                      ///< The wrapped workflow
    size_t threadCount_;                     ///< Number of threads
    bool compiled_ = false;                  ///< Whether compiled

    // ========================================================================
    // Engine Pool (destroyed after the thread pool has joined)
    // ========================================================================

    bool useEnginePool_ = false;                                    ///< Whether using pool
    std::vector<std::unique_ptr<LuaEngine>> enginePoolStorage_;    ///< Engine storage
    std::unique_ptr<EnginePool> enginePool_;                      ///< Engine pool
    std::vector<std::future<void>> pendingTasks_;                  ///< Pending tasks

    // ========================================================================
    // Thread Pool (PIMPL) - declared last so it is destroyed (joined) first
    // ========================================================================

    struct ThreadPoolImpl;
    std::unique_ptr<ThreadPoolImpl> threadPool_;  ///< Thread pool

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
 * @note @p engine, @p context and @p parameters are borrowed and must outlive
 * the call. That holds today because the coroutine's initial_suspend is
 * std::suspend_never, so the body runs to completion before this function
 * returns; anything that makes the body suspend early must take them by value.
 * (@p context is deliberately a reference so the caller observes the writes.)
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
 * Takes the workflow BY VALUE because it assumes ownership of it: the
 * AsyncWorkflow built internally moves from it, and the coroutine frame keeps
 * it alive for the duration. Callers must therefore hand it over explicitly:
 *
 * @code
 * auto task = coExecuteWorkflow(std::move(workflow), engine, params, 4);
 * auto results = task.get();
 * // `workflow` is now moved-from - do not reuse it.
 * @endcode
 *
 * @note @p engine and @p parameters are borrowed and must outlive the call.
 * That holds today because the coroutine's initial_suspend is
 * std::suspend_never, so the body runs to completion before this function
 * returns; anything that makes the body suspend early must take them by value.
 *
 * @param workflow The workflow to execute (ownership is transferred)
 * @param engine The LuaEngine
 * @param parameters The parameters
 * @param threadCount Number of threads
 * @return Async task yielding results
 */
AsyncWorkflowTask coExecuteWorkflow(Workflow workflow,
                                     LuaEngine& engine,
                                     const std::vector<RuleParameter>& parameters,
                                     size_t threadCount);

} // namespace fastrules
