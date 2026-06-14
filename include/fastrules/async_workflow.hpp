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
#include <atomic>

#include "rule.hpp"
#include "rule_result.hpp"
#include "workflow.hpp"
#include "lockfree_engine_pool.hpp"

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;

// ============================================================================
// Async Rule Execution Types
// ============================================================================

struct AsyncRuleResult {
    RuleResult result;
    std::exception_ptr exception;
    
    [[nodiscard]] bool isSuccess() const noexcept {
        return result.isSuccess() && !exception;
    }
};

struct AsyncRulePromise {
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
    ~AsyncRulePromise() { if (handle_) handle_.destroy(); }
    
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
        if (handle_.done()) return handle_.promise().result;
        handle_.resume();
        return handle_.promise().result;
    }
    
private:
    handle_type handle_;
};

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

// ============================================================================
// AsyncWorkflow Class
// ============================================================================

class AsyncWorkflow {
public:
    explicit AsyncWorkflow(size_t threadCount = std::thread::hardware_concurrency());
    explicit AsyncWorkflow(Workflow&& workflow, size_t threadCount = std::thread::hardware_concurrency());
    ~AsyncWorkflow();
    
    AsyncWorkflow(const AsyncWorkflow&) = delete;
    AsyncWorkflow& operator=(const AsyncWorkflow&) = delete;
    AsyncWorkflow(AsyncWorkflow&&) noexcept;
    AsyncWorkflow& operator=(AsyncWorkflow&&) noexcept;
    
    Workflow& workflow() noexcept { return workflow_; }
    [[nodiscard]] const Workflow& workflow() const noexcept { return workflow_; }
    
    void compile(LuaEngine& engine);
    [[nodiscard]] bool isCompiled() const noexcept { return compiled_; }
    
    [[nodiscard]] std::vector<AsyncRuleResult> executeParallelAsync(
        LuaEngine& engine,
        const std::vector<RuleParameter>& parameters);
    
    void waitForCompletion();
    
private:
    Workflow workflow_;
    bool compiled_ = false;
    std::vector<std::future<void>> pendingTasks_;
    
    std::vector<std::unique_ptr<LuaEngine>> enginePoolStorage_;
    std::unique_ptr<LockFreeEnginePool> enginePool_;
    bool useEnginePool_ = false;
    
    size_t threadCount_;
    
    // Thread pool - using unique_ptr to avoid header bloat
    struct ThreadPoolImpl;
    std::unique_ptr<ThreadPoolImpl> threadPool_;
    
    LuaEngine* acquireEngine();
    void releaseEngine(LuaEngine* engine);
};

// Standalone functions
[[nodiscard]] AsyncRulePromise coExecuteRule(std::shared_ptr<Rule> rule,
                                             LuaEngine& engine,
                                             RuleContext& context,
                                             const std::vector<RuleParameter>& parameters);

[[nodiscard]] AsyncWorkflowTask coExecuteWorkflow(Workflow& workflow,
                                                     LuaEngine& engine,
                                                     const std::vector<RuleParameter>& parameters,
                                                     size_t threadCount = std::thread::hardware_concurrency());

} // namespace fastrules
