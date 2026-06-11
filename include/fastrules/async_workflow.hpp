#pragma once

#include <coroutine>
#include <future>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "rule.hpp"
#include "rule_result.hpp"
#include "workflow.hpp"

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;

// ============================================================================
// C++20 Coroutine Support for Async Workflow Execution
// ============================================================================

// Async rule execution result - awaitable type
struct AsyncRuleResult {
    RuleResult result;
    std::exception_ptr exception;
    
    [[nodiscard]] bool isSuccess() const noexcept {
        return result.isSuccess() && !exception;
    }
};

// Promise type for async rule execution
struct AsyncRulePromise {
public:
    struct promise_type {
        AsyncRuleResult result;
        
        auto get_return_object() {
            return AsyncRulePromise{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(AsyncRuleResult value) {
            result = std::move(value);
        }
        
        void unhandled_exception() {
            result.exception = std::current_exception();
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    AsyncRulePromise() = default;
    explicit AsyncRulePromise(handle_type h) : handle_(h) {}
    
    ~AsyncRulePromise() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    // Delete copy, allow move
    AsyncRulePromise(const AsyncRulePromise&) = delete;
    AsyncRulePromise& operator=(const AsyncRulePromise&) = delete;
    
    AsyncRulePromise(AsyncRulePromise&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    AsyncRulePromise& operator=(AsyncRulePromise&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    [[nodiscard]] AsyncRuleResult get() {
        if (handle_.done()) {
            return handle_.promise().result;
        }
        // Wait for completion
        handle_.resume();
        return handle_.promise().result;
    }
    
private:
    handle_type handle_;
};

// ============================================================================
// AsyncWorkflow - C++20 Coroutine-based Workflow Execution
// ============================================================================

// Awaitable type for async rule execution
class RuleAwaitable {
public:
    friend class AsyncWorkflow;
    friend class Rule;
    RuleAwaitable(std::shared_ptr<Rule> rule, 
                  LuaEngine& engine, 
                  RuleContext& context, 
                  const std::vector<RuleParameter>& params);
    
    friend class AsyncWorkflow;
    
    [[nodiscard]] bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) {
        // Execute rule asynchronously
        future_ = std::async(std::launch::async, [this, handle]() mutable {
            try {
                result_ = rule_->execute(engine_, context_, parameters_);
            } catch (...) {
                exception_ = std::current_exception();
            }
            handle.resume();
        });
    }
    
    [[nodiscard]] RuleResult await_resume() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return result_;
    }
    
private:
    std::shared_ptr<Rule> rule_;
    LuaEngine& engine_;
    RuleContext& context_;
    std::vector<RuleParameter> parameters_;
    
    std::future<void> future_;
    RuleResult result_;
    std::exception_ptr exception_;
};

// ============================================================================
// AsyncWorkflow Class
// ============================================================================
// Provides persistent thread pool and engine clones for high-throughput async
// execution. Use this instead of Workflow::executeParallel for repeated
// executions or when you need async/non-blocking behavior.
//
// For guidance on when to use AsyncWorkflow vs executeParallel, see:
// docs/parallel-execution.md
// ============================================================================

class AsyncWorkflow {
public:
    AsyncWorkflow();
    explicit AsyncWorkflow(Workflow&& workflow);
    ~AsyncWorkflow();
    
    // Disable copy, enable move
    AsyncWorkflow(const AsyncWorkflow&) = delete;
    AsyncWorkflow& operator=(const AsyncWorkflow&) = delete;
    AsyncWorkflow(AsyncWorkflow&&) noexcept;
    AsyncWorkflow& operator=(AsyncWorkflow&&) noexcept;
    
    // Properties
    Workflow& workflow() noexcept { return workflow_; }
    [[nodiscard]] const Workflow& workflow() const noexcept { return workflow_; }
    
    // Compile the workflow for async execution
    void compile(LuaEngine& engine);
    [[nodiscard]] bool isCompiled() const noexcept { return compiled_; }
    
    // Async execution using coroutines
    // Returns an awaitable that can be co_await'd
    [[nodiscard]] auto executeAsync(LuaEngine& engine, const std::vector<RuleParameter>& parameters);
    
    // Execute a single rule asynchronously (co_await support)
    [[nodiscard]] auto executeRuleAsync(std::shared_ptr<Rule> rule,
                                         LuaEngine& engine,
                                         RuleContext& context,
                                         const std::vector<RuleParameter>& parameters);
    
    // Parallel execution of independent rules using coroutines
    [[nodiscard]] std::vector<AsyncRuleResult> executeParallelAsync(
        LuaEngine& engine,
        const std::vector<RuleParameter>& parameters);
    
    // Wait for all async operations to complete
    void waitForCompletion();
    
private:
    Workflow workflow_;
    bool compiled_ = false;
    std::vector<std::future<void>> pendingTasks_;
    
public:
    // Thread pool for parallel execution (public so external code can use it)
    class ThreadPool;
    std::unique_ptr<ThreadPool> threadPool_;
    
    [[nodiscard]] ThreadPool& getThreadPool() const { return *threadPool_; }
};

// Forward declaration for AsyncWorkflow::ThreadPool
class AsyncWorkflow::ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename Func, typename... Args>
    auto enqueue(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        using return_type = std::invoke_result_t<Func, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    [[nodiscard]] size_t size() const noexcept { return workers_.size(); }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_ = false;
};

// ============================================================================
// Coroutine Types for Workflow
// ============================================================================

// The return type for co_await on async workflow execution
class AsyncWorkflowTask {
public:
    struct promise_type {
        std::vector<RuleResult> results;
        std::exception_ptr exception;
        
        auto get_return_object() {
            return AsyncWorkflowTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
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
    
    ~AsyncWorkflowTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    // Delete copy, allow move
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
    
    [[nodiscard]] bool await_ready() const noexcept { return handle_.done(); }
    
    void await_suspend(std::coroutine_handle<>) {}
    
    [[nodiscard]] std::vector<RuleResult> await_resume() {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return std::move(handle_.promise().results);
    }
    
private:
    handle_type handle_ = nullptr;
};

// Standalone async functions for rule execution
[[nodiscard]] AsyncRulePromise coExecuteRule(std::shared_ptr<Rule> rule,
                                             LuaEngine& engine,
                                             RuleContext& context,
                                             const std::vector<RuleParameter>& parameters);

// Standalone coroutine for workflow execution
[[nodiscard]] AsyncWorkflowTask coExecuteWorkflow(Workflow& workflow,
                                                     LuaEngine& engine,
                                                     const std::vector<RuleParameter>& parameters);

} // namespace fastrules
