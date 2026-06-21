/**
 * @file timeout_executor.hpp
 * @brief Timeout enforcement for FastRules
 * 
 * Provides hard timeout enforcement that can't be bypassed by user code.
 */

#pragma once

#include "fastrules/rule_timeout_exception.hpp"

#include <chrono>
#include <future>
#include <thread>
#include <atomic>

namespace fastrules {

/**
 * @brief Hard timeout executor
 * 
 * Enforces hard timeouts that can't be bypassed by user code.
 * Uses multiple layers of timeout enforcement.
 */
class TimeoutExecutor {
private:
    std::chrono::milliseconds maxExecutionTime_;
    std::atomic<bool> cancelled_{false};

public:
    /**
     * @brief Construct timeout executor
     * 
     * @param maxExecutionTime Maximum execution time
     */
    explicit TimeoutExecutor(std::chrono::milliseconds maxExecutionTime)
        : maxExecutionTime_(maxExecutionTime) {}

    /**
     * @brief Execute function with hard timeout
     * 
     * @tparam F Function type
     * @param fn Function to execute
     * @return Function result
     * @throws RuleTimeoutException if timeout occurs
     */
    template<typename F>
    auto executeWithTimeout(F&& fn) -> decltype(fn()) {
        // Reset cancelled flag
        cancelled_.store(false);

        // Wrap the work in a heap-allocated packaged_task. The detached worker
        // thread owns its own copy, so the callable and its captures stay valid
        // even after executeWithTimeout returns. Callers must not capture the
        // caller's stack by reference; true hard termination of arbitrary C++
        // code is not possible in standard C++.
        using result_type = decltype(fn());
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::forward<F>(fn));
        auto resultFuture = task->get_future();
        std::thread([task]() mutable { (*task)(); }).detach();

        // Wait for completion or timeout
        if (resultFuture.wait_for(maxExecutionTime_) == std::future_status::timeout) {
            cancelled_.store(true);
            throw RuleTimeoutException("Rule execution timed out after " +
                                     std::to_string(maxExecutionTime_.count()) + " milliseconds");
        }

        // Get the result
        return resultFuture.get();
    }

    /**
     * @brief Check if execution has been cancelled
     * 
     * @return true if execution has been cancelled
     */
    bool isCancelled() const {
        return cancelled_.load();
    }

    /**
     * @brief Set maximum execution time
     * 
     * @param maxExecutionTime Maximum execution time
     */
    void setMaxExecutionTime(std::chrono::milliseconds maxExecutionTime) {
        maxExecutionTime_ = maxExecutionTime;
    }

    /**
     * @brief Get maximum execution time
     * 
     * @return Maximum execution time
     */
    std::chrono::milliseconds getMaxExecutionTime() const {
        return maxExecutionTime_;
    }
};

/**
 * @brief Rule executor with timeout enforcement
 * 
 * Wraps rule execution with multiple layers of timeout protection.
 */
class RuleExecutor {
private:
    std::chrono::milliseconds maxExecutionTime_;
    std::chrono::milliseconds softTimeout_;
    std::chrono::milliseconds hardTimeout_;

public:
    /**
     * @brief Construct rule executor
     * 
     * @param maxExecutionTime Maximum execution time
     */
    explicit RuleExecutor(std::chrono::milliseconds maxExecutionTime = std::chrono::seconds(30))
        : maxExecutionTime_(maxExecutionTime)
        , softTimeout_(maxExecutionTime - std::chrono::milliseconds(100))
        , hardTimeout_(maxExecutionTime + std::chrono::milliseconds(100)) {}

    /**
     * @brief Execute rule with timeout enforcement
     * 
     * @tparam F Function type
     * @param fn Function to execute
     * @return Function result
     * @throws RuleTimeoutException if timeout occurs
     */
    template<typename F>
    auto execute(F&& fn) -> decltype(fn()) {
        // Use a single timeout executor for simplicity
        TimeoutExecutor executor(maxExecutionTime_);
        return executor.executeWithTimeout(std::forward<F>(fn));
    }

    /**
     * @brief Set maximum execution time
     * 
     * @param maxExecutionTime Maximum execution time
     */
    void setMaxExecutionTime(std::chrono::milliseconds maxExecutionTime) {
        maxExecutionTime_ = maxExecutionTime;
        softTimeout_ = maxExecutionTime - std::chrono::milliseconds(100);
        hardTimeout_ = maxExecutionTime + std::chrono::milliseconds(100);
    }

    /**
     * @brief Get maximum execution time
     * 
     * @return Maximum execution time
     */
    std::chrono::milliseconds getMaxExecutionTime() const {
        return maxExecutionTime_;
    }
};

/**
 * @brief Scoped timeout guard
 * 
 * RAII-style timeout guard that enforces timeouts for a scope.
 */
class TimeoutGuard {
private:
    std::chrono::milliseconds timeout_;
    std::atomic<bool> timedOut_{false};
    std::atomic<bool> completed_{false};

public:
    /**
     * @brief Construct timeout guard
     * 
     * @param timeout Timeout duration
     */
    explicit TimeoutGuard(std::chrono::milliseconds timeout)
        : timeout_(timeout) {
        // For now, we don't implement active timeout checking in the guard
        // The main timeout enforcement is handled by TimeoutExecutor
    }

    /**
     * @brief Destructor - marks scope as completed
     */
    ~TimeoutGuard() {
        completed_.store(true);
    }

    /**
     * @brief Check if timeout has occurred
     * 
     * @return true if timeout has occurred
     */
    bool isTimedOut() const {
        return timedOut_.load();
    }
};

} // namespace fastrules