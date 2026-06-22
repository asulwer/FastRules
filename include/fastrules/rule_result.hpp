/**
 * @file rule_result.hpp
 * @brief Rule execution result with timing, metrics, and child results
 * 
 * RuleResult captures the outcome of a single rule execution, including:
 * - Success/failure status
 * - Exception information
 * - Execution timing
 * - Performance metrics
 * - Child rule results (for hierarchical rules)
 * 
 * The result is returned by Rule::execute() and stored in RuleContext
 * for subsequent rules to access.
 * 
 * Result States:
 * - success=true: Rule expression evaluated to true, action executed (if any)
 * - success=false, skipped=false: Rule expression evaluated to false
 * - success=false, skipped=true: Rule was inactive or dependency failed
 * - exception has value: An error occurred during execution
 * 
 * Example:
 * @code
 * auto result = rule.execute(engine, context, params);
 * 
 * if (result.isSuccess()) {
 *     std::cout << "Rule passed in " 
 *               << result.metrics.totalExecutionTime.count() << "ms\n";
 * } else if (result.skipped) {
 *     std::cout << "Rule was skipped\n";
 * } else if (result.exception.has_value()) {
 *     std::cerr << "Rule failed: " << result.exception->what() << "\n";
 * }
 * @endcode
 */

#pragma once

#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <coroutine>

namespace fastrules {

/**
 * @brief Performance and execution metrics for a rule
 * 
 * Tracks various metrics about rule execution:
 * - How many times the rule was evaluated
 * - How many times it failed
 * - Total execution time
 * - When it was last executed
 * 
 * These metrics are useful for:
 * - Performance monitoring
 * - Debugging slow rules
 * - Capacity planning
 */
struct RuleMetrics {
    int evaluationCount = 0;   ///< Number of times rule was evaluated
    int failureCount = 0;      ///< Number of failures
    
    /// @brief Total time spent executing this rule (including children)
    std::chrono::steady_clock::duration totalExecutionTime{0};
    
    /// @brief When the rule was last executed
    std::chrono::steady_clock::time_point lastExecuted;
};

/**
 * @brief Exception type stored in RuleResult
 * 
 * Wraps exception information in a copyable format.
 * Standard exceptions can't be stored directly because
 * they may be sliced when catching by value.
 */
class RuleException : public std::exception {
public:
    /**
     * @brief Construct from a message string
     * @param msg The error message
     */
    explicit RuleException(std::string msg) : message_(std::move(msg)) {}
    
    /// @brief Copy constructor
    RuleException(const RuleException&) = default;
    
    /// @brief Copy assignment
    RuleException& operator=(const RuleException&) = default;
    
    /// @brief Move constructor
    RuleException(RuleException&&) = default;
    
    /// @brief Move assignment
    RuleException& operator=(RuleException&&) = default;

    /// @brief Get the error message
    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;  ///< Stored error message
};

/**
 * @brief Result of a single rule execution
 * 
 * RuleResult is returned by Rule::execute() and contains the complete
 * outcome of the execution. It is designed to be:
 * - Copyable (for storage in context)
 * - Serializable (for extensions that support persistence)
 * - Queryable (rich API for checking various states)
 * 
 * The result includes timing information automatically captured
 * by the execution engine.
 */
struct RuleResult {
    // ========================================================================
    // Identity
    // ========================================================================
    
    std::string ruleName;  ///< Human-readable rule name
    int ruleId = 0;        ///< Rule ID

    // ========================================================================
    // Outcome
    // ========================================================================
    
    /**
     * @brief Whether the rule succeeded
     * 
     * A rule succeeds if:
     * - It has no expression, OR
     * - Its expression evaluates to a truthy value
     * 
     * A rule fails if:
     * - Its expression evaluates to falsy, OR
     * - An exception is thrown during execution
     */
    bool success = false;

    /**
     * @brief Whether the rule was skipped
     * 
     * Rules are skipped when:
     * - The rule is inactive (isActive=false)
     * - A dependency rule failed
     * 
     * Skipped rules don't count as failures but also don't
     * contribute to workflow success.
     */
    bool skipped = false;

    /**
     * @brief Exception information if an error occurred
     * 
     * Set when:
     * - Lua compilation fails (shouldn't happen if compiled)
     * - Lua execution throws an error
     * - Rate limit exceeded
     * - Timeout exceeded
     * - Any other runtime error
     * 
     * Use isSuccess() to check both success and exception.
     */
    std::optional<RuleException> exception;

    // ========================================================================
    // Timing
    // ========================================================================
    
    /// @brief When execution started
    std::chrono::steady_clock::time_point executedAt;
    
    /// @brief When execution completed
    std::chrono::steady_clock::time_point completedAt;

    // ========================================================================
    // Metrics
    // ========================================================================
    
    /// @brief Performance metrics for this execution
    RuleMetrics metrics;

    // ========================================================================
    // Hierarchy
    // ========================================================================
    
    /**
     * @brief Results from child rule executions
     * 
     * If the rule has childRules, this vector contains the results
     * from executing each child. Children execute before the parent
     * and their results are available in the context during parent
     * evaluation.
     */
    std::vector<RuleResult> childResults;

    // ========================================================================
    // Query Methods
    // ========================================================================
    
    /**
     * @brief Check if the rule execution succeeded
     * 
     * A rule is considered successful if:
     * - success is true
     * - No exception occurred
     * - It was not skipped
     * 
     * This is a convenience method that checks all conditions.
     * 
     * @return true if the rule succeeded, false otherwise
     */
    [[nodiscard]] bool isSuccess() const noexcept {
        return success && !exception.has_value() && !skipped;
    }

    /**
     * @brief Check if the rule and all children succeeded
     * 
     * Recursively checks that this rule and all child rules succeeded.
     * Useful for hierarchical rules where parent success depends on
     * all children succeeding.
     * 
     * @return true if this and all children succeeded, false otherwise
     */
    [[nodiscard]] bool isFullySuccessful() const noexcept;

    /**
     * @brief Get the total execution duration
     * 
     * Convenience method that calculates duration from
     * executedAt to completedAt.
     * 
     * @return Execution duration
     */
    [[nodiscard]] std::chrono::nanoseconds duration() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            completedAt - executedAt);
    }
};

/**
 * @brief Result wrapper for async rule execution
 * 
 * Extends RuleResult with exception_ptr for async operations.
 * When executing rules asynchronously, exceptions can't be thrown
 * directly and must be captured in exception_ptr for rethrowing
 * on the calling thread.
 */
struct AsyncRuleResult {
    RuleResult result;                     ///< The actual rule result
    std::exception_ptr exception;          ///< Captured exception for async
    std::chrono::steady_clock::time_point completedAt;  ///< Completion timestamp

    /**
     * @brief Check if the async result represents a success
     * 
     * @return true if no async exception and the result succeeded
     */
    [[nodiscard]] bool isSuccess() const noexcept {
        return !exception && result.isSuccess();
    }
};

/**
 * @brief Future/promise types for async workflows
 */
template<typename T>
struct AsyncTask {
    struct promise_type {
        T value;
        
        auto get_return_object() { return AsyncTask{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        auto initial_suspend() { return std::suspend_never{}; }
        auto final_suspend() noexcept { return std::suspend_never{}; }
        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { std::terminate(); }
    };
    
    std::coroutine_handle<promise_type> handle;
    explicit AsyncTask(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~AsyncTask() { if (handle) handle.destroy(); }
    
    T get() { 
        if (handle) {
            T result = std::move(handle.promise().value);
            return result;
        }
        return T{};
    }
};

/**
 * @brief Async return type for rule results
 * 
 * Provides awaitable semantics for async rule execution.
 */
using AsyncRulePromise = AsyncTask<AsyncRuleResult>;

} // namespace fastrules
