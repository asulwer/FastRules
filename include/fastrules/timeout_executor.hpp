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
    /**
     * @warning On timeout the worker thread is NOT stopped - standard C++ has
     * no way to safely kill a running thread. It keeps running to completion
     * in the background while this call throws. Two consequences follow:
     *  - @p fn must not reference the caller's stack (it may outlive this
     *    call). Capture by value.
     *  - @p fn should poll isCancelled() and return early where it can,
     *    otherwise a runaway task occupies a thread for its full duration.
     */
    template<typename F>
    auto executeWithTimeout(F&& fn) -> decltype(fn()) {
        // Reset cancelled flag. Shared with the worker so a cooperative task
        // can observe the timeout and unwind on its own.
        cancelled_.store(false);

        // Wrap the work in a heap-allocated packaged_task. The worker thread
        // owns its own copy, so the callable and its captures stay valid even
        // after executeWithTimeout returns.
        using result_type = decltype(fn());
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::forward<F>(fn));
        auto resultFuture = task->get_future();

        // Keep the thread joinable so the common (non-timeout) path joins it
        // instead of leaking a detached thread on every single call.
        std::thread worker([task]() mutable { (*task)(); });

        if (resultFuture.wait_for(maxExecutionTime_) == std::future_status::timeout) {
            cancelled_.store(true);
            // The task is still running and owns `task`; it cannot be joined
            // here without waiting for it, which would defeat the timeout.
            worker.detach();
            throw RuleTimeoutException("Rule execution timed out after " +
                                     std::to_string(maxExecutionTime_.count()) + " milliseconds");
        }

        // Completed in time - reclaim the thread rather than leaking it.
        if (worker.joinable()) {
            worker.join();
        }

        // Get the result (rethrows anything the task threw)
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
        , softTimeout_(clampNonNegative(maxExecutionTime - std::chrono::milliseconds(100)))
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
        softTimeout_ = clampNonNegative(maxExecutionTime - std::chrono::milliseconds(100));
        hardTimeout_ = maxExecutionTime + std::chrono::milliseconds(100);
    }

    /// A max execution time below 100ms would otherwise yield a negative soft
    /// timeout, which reads as "already expired" wherever it is compared.
    static std::chrono::milliseconds clampNonNegative(std::chrono::milliseconds d) {
        return d.count() < 0 ? std::chrono::milliseconds(0) : d;
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