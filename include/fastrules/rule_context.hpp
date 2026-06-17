/**
 * @file rule_context.hpp
 * @brief Execution context for rule evaluation
 * 
 * RuleContext provides storage for intermediate results and variables
 * during rule execution. It enables:
 * - Rules accessing results of previously executed rules
 * - Context variables for passing data between rules
 * - Error tracking for debugging
 * 
 * Thread Safety:
 * RuleContext is thread-safe using a shared_mutex:
 * - Multiple threads can read concurrently
 * - Writes are exclusive
 * 
 * In Workflow execution, each execution uses a single RuleContext that
 * is shared across all rule executions. This allows later rules to
 * access results of earlier rules.
 * 
 * Example:
 * @code
 * fastrules::RuleContext ctx;
 * 
 * // Execute first rule
 * auto result1 = rule1.execute(engine, ctx, params);
 * 
 * // Second rule can access first rule's result
 * // Expression: "context.getResult('rule1').success"
 * auto result2 = rule2.execute(engine, ctx, params);
 * @endcode
 */

#pragma once

#include "fastrules/rule_result.hpp"

#include <string>
#include <any>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include <utility>

namespace fastrules {

/**
 * @brief Context for rule execution - stores results and variables
 * 
 * The RuleContext is passed to each rule during execution and serves as
 * a shared state container. It allows:
 * 
 * 1. Result Storage: Each rule's result is stored by rule name,
 *    making it accessible to subsequent rules.
 * 
 * 2. Variable Passing: Arbitrary C++ values can be stored and
 *    retrieved using context variables.
 * 
 * 3. Error Tracking: The last error is tracked for debugging.
 * 
 * Access from Lua:
 * Results are accessible in Lua expressions via context.getResult():
 * @code
 * -- Check if previous rule succeeded
 * context.getResult("validate_age").success
 * 
 * -- Get the rule ID from result
 * context.getResult("validate_age").ruleId
 * @endcode
 * 
 * Lifetime:
 * A RuleContext should be created at the start of workflow execution
 * and destroyed when execution completes. It is not persistent.
 */
class RuleContext {
public:
    /// @brief Default constructor
    RuleContext() = default;
    
    /// @brief Destructor
    ~RuleContext() = default;
    
    /// @brief Copy constructor
    RuleContext(const RuleContext& other);
    
    /// @brief Copy assignment
    RuleContext& operator=(const RuleContext& other);
    
    /// @brief Move constructor
    RuleContext(RuleContext&&) = default;
    
    /// @brief Move assignment
    RuleContext& operator=(RuleContext&&) = default;

    // ========================================================================
    // Result Management
    // ========================================================================
    
    /**
     * @brief Store a rule execution result
     * 
     * Stores the result using the rule's name as the key.
     * If a result already exists for this rule, it is overwritten.
     * 
     * @param ruleName The name of the rule that produced the result
     * @param result The RuleResult to store
     * 
     * Thread Safety: Thread-safe (uses unique_lock)
     */
    void setResult(const std::string& ruleName, const RuleResult& result);

    /**
     * @brief Retrieve a stored rule result
     * 
     * Looks up a result by rule name. Returns std::nullopt if not found.
     * 
     * @param ruleName The name of the rule to look up
     * @return The RuleResult if found, std::nullopt otherwise
     * 
     * Thread Safety: Thread-safe (uses shared_lock)
     */
    [[nodiscard]] std::optional<RuleResult> getResult(const std::string& ruleName) const;

    /**
     * @brief Check if a result exists for a rule
     * 
     * @param ruleName The name of the rule to check
     * @return true if a result exists, false otherwise
     * 
     * Thread Safety: Thread-safe (uses shared_lock)
     */
    [[nodiscard]] bool hasResult(const std::string& ruleName) const;

    // ========================================================================
    // Variable Management
    // ========================================================================
    
    /**
     * @brief Store a context variable
     * 
     * Variables can store any C++ value wrapped in std::any.
     * Variables are not directly accessible from Lua - they are
     * for C++-side communication between rules.
     * 
     * @param name The variable name
     * @param value The value to store (any C++ type)
     * 
     * Thread Safety: Thread-safe (uses unique_lock)
     * 
     * Example:
     * @code
     * ctx.setVariable("customer_id", 12345);
     * ctx.setVariable("customer", customerObj);
     * @endcode
     */
    void setVariable(const std::string& name, std::any value);

    /**
     * @brief Retrieve a context variable
     * 
     * @param name The variable name
     * @return The variable value if found, std::nullopt otherwise
     * 
     * Thread Safety: Thread-safe (uses shared_lock)
     * 
     * Example:
     * @code
     * auto val = ctx.getVariable("customer_id");
     * if (val.has_value()) {
     *     int id = std::any_cast<int>(val.value());
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<std::any> getVariable(const std::string& name) const;

    // ========================================================================
    // Error Tracking
    // ========================================================================
    
    /**
     * @brief Set the last error information
     * 
     * Called automatically by Rule::execute when an error occurs.
     * 
     * @param ruleName The name of the rule that failed
     * @param error The error message
     * 
     * Thread Safety: Thread-safe (uses unique_lock)
     */
    void setLastError(const std::string& ruleName, const std::string& error);

    /**
     * @brief Get the last error information
     * 
     * @return A pair of (ruleName, errorMessage) if an error occurred,
     *         std::nullopt otherwise
     * 
     * Thread Safety: Thread-safe (uses shared_lock)
     */
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> getLastError() const;

    /**
     * @brief Clear the last error
     * 
     * Thread Safety: Thread-safe (uses unique_lock)
     */
    void clearLastError();

    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /**
     * @brief Clear all results and variables
     * 
     * Resets the context to its initial state.
     * Useful when reusing a context for multiple workflow executions.
     * 
     * Thread Safety: Thread-safe (uses unique_lock)
     */
    void clear();

private:
    /// @brief Map of rule names to their results
    std::unordered_map<std::string, RuleResult> results_;
    
    /// @brief Map of variable names to values
    std::unordered_map<std::string, std::any> variables_;
    
    /// @brief Last error (rule name, message)
    std::optional<std::pair<std::string, std::string>> lastError_;
    
    /// @brief Shared mutex for thread-safe access
    mutable std::shared_mutex mutex_;
};

} // namespace fastrules
