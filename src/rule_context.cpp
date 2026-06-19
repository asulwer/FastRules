/**
 * @file rule_context.cpp
 * @brief Rule execution context implementation
 * 
 * The RuleContext provides thread-safe storage for rule execution
 * results and variables. It acts as a shared state container that
 * rules can read from and write to during execution.
 * 
 * Design Pattern:
 * - Uses shared_mutex for reader-writer locking
 * - Allows concurrent reads from multiple rules
 * - Exclusive lock required for writes
 * 
 * Use Cases:
 * - Storing rule results for dependency checking
 * - Passing data between rules in a workflow
 * - Sharing computed values across expression evaluations
 * - Error tracking for debugging
 * 
 * Thread Safety:
 * - All methods are thread-safe
 * - Read operations use shared_lock (concurrent reads allowed)
 * - Write operations use unique_lock (exclusive access)
 * - Safe to share a single RuleContext across parallel rule execution
 * 
 * Lifetime:
 * - RuleContext should outlive the rule execution that uses it
 * - Typically created per-workflow-execution
 * - Can be reused for multiple workflow executions (call clear())
 */

#include "fastrules/rule_context.hpp"

#include <shared_mutex>

namespace fastrules {

// ========================================================================
// Copy Constructor and Assignment
// ========================================================================

RuleContext::RuleContext(const RuleContext& other) {
    std::shared_lock lock(other.mutex_);
    results_ = other.results_;
    variables_ = other.variables_;
    lastError_ = other.lastError_;
}

RuleContext& RuleContext::operator=(const RuleContext& other) {
    if (this != &other) {
        // Lock both contexts to prevent deadlock
        // We need to be careful about locking order to avoid deadlock
        // For simplicity, we'll lock the other context first
        std::shared_lock otherLock(other.mutex_);
        std::unique_lock thisLock(mutex_);
        
        results_ = other.results_;
        variables_ = other.variables_;
        lastError_ = other.lastError_;
    }
    return *this;
}

// ========================================================================
// Result Management
// ========================================================================

/**
 * @brief Store a rule execution result
 * 
 * Thread-safe write using unique_lock. Overwrites any existing
 * result with the same rule name.
 * 
 * @param ruleName Name of the rule that produced the result
 * @param result The RuleResult to store
 */
void RuleContext::setResult(const std::string& ruleName, const RuleResult& result) {
    std::unique_lock lock(mutex_);
    results_[ruleName] = result;
}

/**
 * @brief Retrieve a rule execution result
 * 
 * Thread-safe read using shared_lock. Returns std::nullopt if
 * no result exists for the given rule name.
 * 
 * @param ruleName Name of the rule to look up
 * @return Optional containing the RuleResult, or nullopt if not found
 */
std::optional<RuleResult> RuleContext::getResult(const std::string& ruleName) const {
    std::shared_lock lock(mutex_);
    auto it = results_.find(ruleName);
    if (it != results_.end()) {
        return it->second;
    }
    return std::nullopt;
}

/**
 * @brief Check if a result exists for a rule
 * 
 * Convenience method that returns true without copying the result.
 * More efficient than getResult() when you only need existence.
 * 
 * @param ruleName Name of the rule to check
 * @return true if a result exists, false otherwise
 */
bool RuleContext::hasResult(const std::string& ruleName) const {
    std::shared_lock lock(mutex_);
    return results_.contains(ruleName);
}

/**
 * @brief Store a variable value
 * 
 * Variables are arbitrary std::any values that can be shared
 * between rules. Useful for computed values or temporary state.
 * 
 * @param name Variable name (acts as key)
 * @param value The value to store (moved into storage)
 */
void RuleContext::setVariable(const std::string& name, std::any value) {
    std::unique_lock lock(mutex_);
    variables_[name] = std::move(value);
}

/**
 * @brief Retrieve a variable value
 * 
 * Returns a copy of the stored std::any. The caller is responsible
 * for any_cast to the expected type.
 * 
 * @param name Variable name to look up
 * @return Optional containing the value, or nullopt if not found
 */
std::optional<std::any> RuleContext::getVariable(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return std::nullopt;
}

/**
 * @brief Clear all stored results and variables
 * 
 * Resets the context to empty state. Useful for reusing a
 * RuleContext across multiple workflow executions.
 */
void RuleContext::clear() {
    std::unique_lock lock(mutex_);
    results_.clear();
    variables_.clear();
}

/**
 * @brief Store the last error that occurred
 * 
 * Provides a way to track the most recent error for debugging.
 * Only stores one error - subsequent calls overwrite.
 * 
 * @param ruleName Name of the rule that failed
 * @param error Error message or description
 */
void RuleContext::setLastError(const std::string& ruleName, const std::string& error) {
    std::unique_lock lock(mutex_);
    lastError_ = {ruleName, error};
}

/**
 * @brief Get the last error that occurred
 * 
 * Returns a pair of (ruleName, errorMessage). Returns nullopt
 * if no error has been set since construction or last clear.
 * 
 * @return Optional pair of rule name and error message
 */
std::optional<std::pair<std::string, std::string>> RuleContext::getLastError() const {
    std::shared_lock lock(mutex_);
    return lastError_;
}

/**
 * @brief Clear the last error
 * 
 * Resets lastError_ to nullopt. Call this after handling an error
 * to prevent stale error reporting.
 */
void RuleContext::clearLastError() {
    std::unique_lock lock(mutex_);
    lastError_.reset();
}

} // namespace fastrules
