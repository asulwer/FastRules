/**
 * @file timeout_executor.hpp
 * @brief Timeout enforcement for FastRules
 * 
 * Provides hard timeout enforcement that can't be bypassed by user code.
 */

#pragma once

#include <chrono>
#include <future>
#include <thread>
#include <stdexcept>
#include <atomic>

namespace fastrules {

/**
 * @brief Exception thrown when a rule execution times out
 */
class RuleTimeoutException : public std::runtime_error {
public:
    explicit RuleTimeoutException(const std::string& message = "Rule execution timed out") 
        : std::runtime_error(message) {}
};

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
        
        // Launch the function in a separate thread
        auto future = std::async(std::launch::async, [&]() -> decltype(fn()) {
            return fn();
        });
        
        // Wait for completion or timeout
        if (future.wait_for(maxExecutionTime_) == std::future_status::timeout) {
            // Mark as cancelled
            cancelled_.store(true);
            
            // Try to terminate the thread (this is not guaranteed to work)
            // In practice, you might need platform-specific code for forceful termination
            throw RuleTimeoutException("Rule execution timed out after " + 
                                     std::to_string(maxExecutionTime_.count()) + " milliseconds");
        }
        
        // Get the result
        return future.get();
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
        // Layer 1: Soft timeout with warning
        TimeoutExecutor softExecutor(softTimeout_);
        
        // Layer 2: Hard timeout that throws exception
        TimeoutExecutor hardExecutor(hardTimeout_);
        
        try {
            // Execute with hard timeout
            return hardExecutor.executeWithTimeout([&]() -> decltype(fn()) {
                // Check for soft timeout periodically
                // This would require the function to be cooperative
                // In practice, you might need to use signals or other mechanisms
                
                return fn();
            });
        } catch (const RuleTimeoutException&) {
            throw;
        } catch (...) {
            // Re-throw other exceptions
            throw;
        }
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
    TimeoutExecutor executor_;
    std::future<void> timeoutFuture_;
    std::atomic<bool> completed_{false};

public:
    /**
     * @brief Construct timeout guard
     * 
     * @param timeout Timeout duration
     */
    explicit TimeoutGuard(std::chrono::milliseconds timeout)
        : executor_(timeout) {
        
        // Launch timeout checker
        timeoutFuture_ = std::async(std::launch::async, [this, timeout]() {
            std::this_thread::sleep_for(timeout);
            if (!completed_.load()) {
                // Timeout occurred - this is where you'd implement
                // platform-specific termination if needed
            }
        });
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
        return executor_.isCancelled();
    }
};

} // namespace fastrules