/**
 * @file rule.hpp
 * @brief Rule definition and execution - the core building block of FastRules
 * 
 * A Rule represents a single condition-action pair in the rules engine.
 * Rules can be simple (just an expression) or complex (with child rules,
 * dependencies, caching, and custom actions).
 * 
 * Rule Execution Flow:
 * 1. Check if rule is active (skip if inactive)
 * 2. Check rate limiting
 * 3. Validate parameter types
 * 4. Execute child rules (bottom-up)
 * 5. Evaluate expression (if all children succeed)
 * 6. Execute action (if expression evaluates to true)
 * 7. Cache result (if caching is enabled)
 * 8. Store result in context
 * 
 * Hierarchical Rules:
 * Rules support a parent-child hierarchy where child rules execute before
 * their parent. If any child fails, the parent aborts without evaluating
 * its expression. This enables complex logic composition.
 * 
 * Dependencies:
 * Rules can depend on other rules by name. The dependency rule must
 * succeed before the dependent rule executes. Dependencies are checked
 * at execution time, not compile time.
 * 
 * Caching:
 * Rules support memoization via the cacheDuration field. Results are cached
 * based on parameter values and invalidated when:
 * - The cache TTL expires
 * - invalidateCache() is called
 * - The rule's cacheGeneration changes
 * 
 * Thread Safety:
 * - Rule construction: NOT thread-safe
 * - Rule compilation: NOT thread-safe (compile once)
 * - Rule execution: Thread-safe if each thread uses its own LuaEngine
 * - Cache operations: Thread-safe (uses mutex internally)
 * 
 * Example:
 * @code
 * // Simple rule
 * fastrules::Rule rule;
 * rule.id = 1;
 * rule.expression = "age >= 18";
 * rule.action = "status = 'adult'";
 * 
 * // Using the Builder pattern
 * auto rule = fastrules::Rule::Builder(1)
 *     .withName("check_age")
 *     .withExpression("age >= 18")
 *     .withAction("log('Adult verified')")
 *     .withPriority(10)
 *     .withCache(std::chrono::seconds(30))
 *     .active(true)
 *     .build();
 * 
 * // Compile and execute
 * fastrules::LuaEngine engine;
 * rule->compile(engine);
 * 
 * fastrules::RuleContext ctx;
 * auto result = rule->execute(engine, ctx, {{"age", 25}});
 * @endcode
 */

#pragma once

#include "fastrules/rule_result.hpp"
#include "fastrules/rate_limiter.hpp"
#include "fastrules/streaming_result.hpp"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <any>
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <typeindex>

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;

// ============================================================================
// Exception Classes
// ============================================================================

/**
 * @brief Exception thrown when rule compilation fails
 * 
 * Contains details about why Lua compilation failed, including:
 * - Syntax errors in expressions/actions
 * - Reference to the offending rule
 * - The expression/action that failed
 */
class RuleCompilationException : public std::runtime_error {
public:
    explicit RuleCompilationException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @brief Exception thrown when rule validation fails
 * 
 * Contains details about validation failures:
 * - Circular dependencies
 * - Duplicate rule IDs
 * - Missing dependencies
 */
class RuleValidationException : public std::runtime_error {
public:
    explicit RuleValidationException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @brief Exception thrown when rule execution times out
 * 
 * Thrown when a rule exceeds its configured timeout.
 * The timeout mechanism uses Lua debug hooks.
 */
class RuleTimeoutException : public std::runtime_error {
public:
    explicit RuleTimeoutException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @brief Exception thrown when rule execution fails
 * 
 * Generic execution failure - could be Lua runtime error,
 * memory exhaustion, or other runtime issues.
 */
class RuleExecutionException : public std::runtime_error {
public:
    explicit RuleExecutionException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @brief Parameter passed to rule execution
 * 
 * RuleParameters bind C++ values to Lua variable names.
 * The name field becomes a global variable in the Lua expression.
 * The type field is optional and used for runtime type validation.
 * 
 * Example:
 * @code
 * // Creates a Lua global 'age' with value 25
 * RuleParameter param("age", 25);
 * 
 * // With explicit type declaration
 * RuleParameter param("customer", customerObj, typeid(Customer));
 * @endcode
 */
struct RuleParameter {
    std::string name;                    ///< Lua variable name
    std::any value;                    ///< C++ value (converted to Lua)
    std::optional<std::type_index> type; ///< Optional type for validation

    /// @brief Construct a parameter with name and value
    template<typename T>
    RuleParameter(std::string n, T v) 
        : name(std::move(n)), value(std::move(v)) {}

    /// @brief Construct with explicit type declaration
    template<typename T>
    RuleParameter(std::string n, T v, std::type_index t) 
        : name(std::move(n)), value(std::move(v)), type(t) {}
};

/**
 * @brief A single rule in the rules engine
 * 
 * Rules are the fundamental building blocks of FastRules workflows.
 * Each rule has:
 * - A unique ID
 * - An optional human-readable name
 * - A Lua expression that evaluates to true/false
 * - An optional Lua action to execute on success
 * - Optional child rules (executed before parent)
 * - Optional dependencies on other rules
 * 
 * The Rule class uses the Builder pattern for convenient construction
 * and supports method chaining for configuration.
 * 
 * Memory Management:
 * - Rules are typically managed via std::shared_ptr<Rule>
 * - The Rule class is designed to be stored in containers
 * - Child rules are stored as shared_ptr to enable sharing
 */
class Rule {
public:
    using Id = int;  ///< Rule identifier type

    // ========================================================================
    // Rule Identity
    // ========================================================================
    
    Id id = 0;                          ///< Unique rule identifier (required)
    std::string name;                   ///< Human-readable name (optional, used for dependencies)
    std::string description;            ///< Description for documentation/debugging

    // ========================================================================
    // Rule Logic
    // ========================================================================
    
    /**
     * @brief Lua expression that evaluates to boolean
     * 
     * The expression is compiled into Lua bytecode and evaluated at runtime.
     * Parameter values are bound to Lua globals before evaluation.
     * 
     * Example expressions:
     * - "age >= 18" - simple comparison
     * - "isNotNull(customer) and customer.age >= 18" - null check with field access
     * - "inRange(score, 0, 100)" - using built-in predicates
     * - "context.getResult('previous_rule').success" - accessing previous results
     */
    std::string expression;

    /**
     * @brief Lua action to execute when expression evaluates to true
     * 
     * Actions are optional. If present, they execute after the expression
     * passes. Actions can modify state, call registered callbacks, or
     * perform side effects.
     * 
     * Example actions:
     * - "log('Rule passed')" - logging
     * - "callbacks.sendEmail(customer.email)" - calling registered callback
     * - "context.approved = true" - setting context variable
     */
    std::string action;

    // ========================================================================
    // Execution Control
    // ========================================================================
    
    /**
     * @brief Execution priority (lower = earlier)
     * 
     * Within a dependency level, rules are sorted by priority.
     * Rules with the same priority execute in undefined order.
     * Default priority is 0.
     */
    int priority = 0;
    
    /**
     * @brief Whether this rule is active
     * 
     * Inactive rules are skipped during execution (no evaluation, no result).
     * This enables dynamic rule sets where rules can be enabled/disabled
     * at runtime without removing them from the workflow.
     */
    bool isActive = true;

    /**
     * @brief Maximum execution time before timeout
     * 
     * Uses Lua debug hooks for preemption. The timeout is enforced
     * cooperatively - very tight loops may exceed the timeout slightly.
     * Default is no timeout.
     * 
     * Note: Timeout requires the Lua state to execute hook checks,
     * which occur every 1000 Lua instructions.
     */
    std::optional<std::chrono::milliseconds> timeout;

    // ========================================================================
    // Hierarchical Rules
    // ========================================================================
    
    /**
     * @brief Child rules executed before this rule
     * 
     * Child rules execute in a bottom-up (bubble-up) pattern:
     * 1. All children execute first
     * 2. If any child fails, parent aborts
     * 3. Only if all children succeed does the parent evaluate
     * 
     * This enables complex logic composition where a rule's validity
     * depends on multiple sub-conditions.
     */
    std::vector<std::shared_ptr<Rule>> childRules;

    // ========================================================================
    // Dependencies
    // ========================================================================
    
    /**
     * @brief Name of rule that must succeed before this rule executes
     * 
     * Dependencies create execution order constraints. If the dependency
     * rule fails or doesn't exist, this rule is skipped.
     * 
     * Dependencies are resolved by name at execution time, allowing
     * dynamic rule relationships.
     */
    std::optional<std::string> dependsOnRuleName;

    // ========================================================================
    // Caching
    // ========================================================================
    
    /**
     * @brief Cache duration for rule results
     * 
     * When set, rule results are memoized based on parameter values.
     * Subsequent executions with the same parameters return the cached
     * result until the TTL expires or the cache is invalidated.
     * 
     * Cache key includes:
     * - Rule ID
     * - Parameter names and values
     * - Cache generation number
     */
    std::optional<std::chrono::milliseconds> cacheDuration;

    // ========================================================================
    // Rate Limiting
    // ========================================================================
    
    /**
     * @brief Optional custom rate limiter for this rule
     * 
     * If not set, uses the global RateLimiter::global() instance.
     * Rate limiting is checked before rule execution and throws
     * RateLimitException if exceeded.
     */
    std::optional<std::shared_ptr<RateLimiter>> rateLimiter;

    // ========================================================================
    // Compilation State (Internal)
    // ========================================================================
    
    /**
     * @brief Whether the rule has been compiled
     * 
     * Set by compile() after successful Lua compilation.
     * Checked by execute() - will auto-compile if false.
     */
    bool isCompiled = false;
    
    /**
     * @brief Whether the rule has been validated
     * 
     * Set by validate() after successful validation passes.
     * Validation includes circular dependency detection and
     * duplicate ID checking.
     */
    bool isValidated = false;

    // ========================================================================
    // Construction
    // ========================================================================
    
    /// @brief Default constructor
    Rule() = default;
    
    /// @brief Virtual destructor for inheritance support
    virtual ~Rule() = default;
    
    /// @brief Move constructor
    Rule(Rule&&) = default;
    
    /// @brief Move assignment
    Rule& operator=(Rule&&) = default;
    
    /// @brief Copy constructor (copies child rules deeply)
    Rule(const Rule& other);
    
    /// @brief Copy assignment (copies child rules deeply)
    Rule& operator=(const Rule& other);

    // ========================================================================
    // Lifecycle Methods
    // ========================================================================
    
    /**
     * @brief Compile the rule's expression and action into Lua bytecode
     * 
     * Must be called before execute(). Compiles the expression and action
     * into Lua bytecode and stores references for execution.
     * 
     * @param engine The LuaEngine to compile against
     * @throws RuleCompilationException if compilation fails
     * 
     * Thread Safety: NOT thread-safe. Call once from a single thread.
     */
    void compile(LuaEngine& engine);

    /**
     * @brief Validate the rule and its relationships
     * 
     * Performs static validation:
     * - Circular dependency detection (DFS)
     * - Duplicate ID checking
     * - Dependency existence verification
     * 
     * @param allRules All rules in the workflow (for dependency checking)
     * @throws RuleValidationException if validation fails
     */
    void validate(const std::vector<std::reference_wrapper<const Rule>>& allRules);

    /**
     * @brief Execute the rule with given parameters
     * 
     * Executes the full rule lifecycle:
     * 1. Check cache
     * 2. Check rate limiting
     * 3. Validate parameters
     * 4. Execute children
     * 5. Evaluate expression
     * 6. Execute action
     * 7. Cache result
     * 
     * @param engine The LuaEngine to execute with
     * @param context The execution context for results/variables
     * @param parameters Parameter values to bind to Lua globals
     * @return RuleResult containing success/failure and metadata
     * 
     * Thread Safety: Thread-safe if each thread uses its own LuaEngine.
     * The Rule itself is not modified during execution.
     */
    RuleResult execute(LuaEngine& engine, RuleContext& context, 
                       const std::vector<RuleParameter>& parameters);

    // ========================================================================
    // Cache Management
    // ========================================================================
    
    /**
     * @brief Invalidate all cached results for this rule
     * 
     * Increments the cache generation counter, effectively invalidating
     * all existing cache entries. Does not clear the cache immediately -
     * entries are lazily removed on next access.
     * 
     * @return The new cache generation number
     */
    int invalidateCache();

    // ========================================================================
    // Dependency Analysis
    // ========================================================================
    
    /**
     * @brief Check if this rule has a circular dependency
     * 
     * Walks the dependency chain starting from this rule's dependsOnRuleName
     * and checks if it eventually leads back to this rule.
     * 
     * @param allRules All rules in the workflow for name-to-ID lookup
     * @return true if a cycle exists, false otherwise
     */
    [[nodiscard]] bool hasCircularDependency(
        const std::vector<std::reference_wrapper<const Rule>>& allRules) const;

    /**
     * @brief Get the chain of dependencies for this rule
     * 
     * Returns a list of rule IDs representing the dependency chain,
     * starting with this rule and following dependsOnRuleName links.
     * 
     * @param allRules All rules in the workflow
     * @return Vector of rule IDs in dependency order
     */
    [[nodiscard]] std::vector<Id> getDependencyChain(
        const std::vector<std::reference_wrapper<const Rule>>& allRules) const;

    // ========================================================================
    // Predicate Factories
    // ========================================================================
    
    /**
     * @brief Create a rule that checks if a parameter is not null
     * @param parameterName The parameter to check
     * @param description Optional description (auto-generated if empty)
     * @return A Rule checking isNotNull(parameterName)
     */
    [[nodiscard]] static Rule isNotNull(const std::string& parameterName,
                                         const std::string& description = "");

    /**
     * @brief Create a rule that checks if parameter > value
     * @param parameterName The parameter to compare
     * @param value The threshold value
     * @param description Optional description
     * @return A Rule checking parameterName > value
     */
    [[nodiscard]] static Rule greaterThan(const std::string& parameterName, double value,
                                           const std::string& description = "");

    /**
     * @brief Create a rule that checks if parameter < value
     * @param parameterName The parameter to compare
     * @param value The threshold value
     * @param description Optional description
     * @return A Rule checking parameterName < value
     */
    [[nodiscard]] static Rule lessThan(const std::string& parameterName, double value,
                                        const std::string& description = "");

    /**
     * @brief Create a rule that checks string equality
     * @param parameterName The parameter to check
     * @param value The expected string value
     * @param description Optional description
     * @return A Rule checking parameterName == "value"
     */
    [[nodiscard]] static Rule equals(const std::string& parameterName, 
                                      const std::string& value,
                                      const std::string& description = "");

    /**
     * @brief Create a rule that checks regex match
     * @param parameterName The parameter to check
     * @param pattern The regex pattern
     * @param description Optional description
     * @return A Rule using matchesRegex()
     */
    [[nodiscard]] static Rule matchesRegex(const std::string& parameterName,
                                            const std::string& pattern,
                                            const std::string& description = "");

    /**
     * @brief Create a rule that checks substring containment
     * @param parameterName The parameter to check
     * @param substring The substring to find
     * @param description Optional description
     * @return A Rule using string.find()
     */
    [[nodiscard]] static Rule contains(const std::string& parameterName,
                                        const std::string& substring,
                                        const std::string& description = "");

    // ========================================================================
    // Builder Pattern
    // ========================================================================
    
    /**
     * @brief Builder class for fluent Rule construction
     * 
     * Provides a fluent API for constructing rules with many optional
     * parameters. All methods return *this for chaining.
     * 
     * Example:
     * @code
     * auto rule = Rule::Builder(1)
     *     .withName("age_check")
     *     .withExpression("age >= 18")
     *     .withPriority(10)
     *     .withCache(std::chrono::seconds(30))
     *     .build();
     * @endcode
     */
    class Builder {
    public:
        /**
         * @brief Construct a builder starting with the rule ID
         * @param id The unique rule identifier
         */
        explicit Builder(Id id);

        /// @brief Set the rule name
        Builder& withName(std::string name);
        
        /// @brief Set the rule description
        Builder& withDescription(std::string desc);
        
        /// @brief Set the Lua expression
        Builder& withExpression(std::string expr);
        
        /// @brief Set the Lua action
        Builder& withAction(std::string act);
        
        /// @brief Set the execution priority
        Builder& withPriority(int prio);
        
        /// @brief Set the active state
        Builder& active(bool active);
        
        /// @brief Set the cache duration
        Builder& withCache(std::chrono::milliseconds duration);
        
        /// @brief Set the timeout duration
        Builder& withTimeout(std::chrono::milliseconds to);
        
        /// @brief Set the dependency rule name
        Builder& dependsOn(std::string ruleName);
        
        /// @brief Add a child rule
        Builder& withChild(std::shared_ptr<Rule> child);
        
        /// @brief Set a custom rate limiter
        Builder& withRateLimiter(std::shared_ptr<RateLimiter> limiter);

        /// @brief Build and return the Rule
        [[nodiscard]] std::shared_ptr<Rule> build();

    private:
        std::shared_ptr<Rule> rule_;  ///< The rule being constructed
    };

    // Inline implementations for Builder
    inline Builder::Builder(Id id) : rule_(std::make_shared<Rule>()) {
        rule_->id_ = id;
    }

    inline Builder& Builder::withName(std::string name) {
        rule_->name_ = std::move(name);
        return *this;
    }

    inline Builder& Builder::withDescription(std::string desc) {
        rule_->description_ = std::move(desc);
        return *this;
    }

    inline Builder& Builder::withExpression(std::string expr) {
        rule_->expression_ = std::move(expr);
        return *this;
    }

    inline Builder& Builder::withAction(std::string act) {
        rule_->action_ = std::move(act);
        return *this;
    }

    inline Builder& Builder::withPriority(int prio) {
        rule_->priority_ = prio;
        return *this;
    }

    inline Builder& Builder::active(bool active) {
        rule_->active_ = active;
        return *this;
    }

    inline Builder& Builder::withCache(std::chrono::milliseconds duration) {
        rule_->cacheDuration_ = duration;
        return *this;
    }

    inline Builder& Builder::withTimeout(std::chrono::milliseconds to) {
        rule_->timeout_ = to;
        return *this;
    }

    inline Builder& Builder::dependsOn(std::string ruleName) {
        rule_->dependsOn_ = std::move(ruleName);
        return *this;
    }

    inline Builder& Builder::withChild(std::shared_ptr<Rule> child) {
        rule_->children_.push_back(std::move(child));
        return *this;
    }

    inline Builder& Builder::withRateLimiter(std::shared_ptr<RateLimiter> limiter) {
        rule_->rateLimiter_ = std::move(limiter);
        return *this;
    }

    inline std::shared_ptr<Rule> Builder::build() {
        return std::move(rule_);
    }

    /**
     * @brief Create a Builder starting with ID and expression
     * 
     * Convenience factory method equivalent to:
     * Builder(id).withExpression(expression).active(active)
     * 
     * @param id The rule ID
     * @param expression The Lua expression
     * @param active Whether the rule is active
     * @return A Builder for further configuration
     */
    [[nodiscard]] static Builder create(Id id, const std::string& expression, bool active = true);

private:
    // ========================================================================
    // Internal Cache Implementation
    // ========================================================================
    
    /**
     * @brief Cache entry containing result and metadata
     */
    struct CacheEntry {
        std::shared_ptr<RuleResult> result;     ///< The cached result
        std::chrono::steady_clock::time_point expiresAt;  ///< Expiration time
        int generation;                          ///< Cache generation when stored
    };

    mutable std::unordered_map<std::string, CacheEntry> cache_;  ///< Cache storage
    mutable std::unique_ptr<std::mutex> cacheMutex_;            ///< Cache mutex
    mutable int cacheGeneration_ = 0;                             ///< Current cache generation

    /**
     * @brief Build a cache key from parameters
     * 
     * Creates a unique string key based on:
     * - Rule ID
     * - Parameter names and values (type-specific serialization)
     * 
     * @param parameters The parameters to hash
     * @return A unique cache key
     */
    [[nodiscard]] std::string buildCacheKey(const std::vector<RuleParameter>& parameters) const;

    /**
     * @brief Store a result in the cache
     * @param parameters The parameters used (for cache key)
     * @param result The result to cache
     */
    void storeInCache(const std::vector<RuleParameter>& parameters, 
                      const RuleResult& result) const;

    // ========================================================================
    // Compiled References (Per-Engine)
    // ========================================================================
    
    /**
     * @brief Map of LuaEngine* to compiled expression reference
     * 
     * Each engine has its own Lua state, so compiled references
     * are tracked per-engine. The default ref is used when no
     * engine-specific ref exists.
     */
    std::unordered_map<LuaEngine*, int> engineExpressionRefs_;
    std::unordered_map<LuaEngine*, int> engineActionRefs_;
    std::optional<int> compiledExpressionRef;  ///< Default compiled expression ref
    std::optional<int> compiledActionRef;     ///< Default compiled action ref

    /**
     * @brief Get the compiled expression reference for an engine
     * @param engine The LuaEngine
     * @return The reference ID, or -1 if not compiled
     */
    int getExpressionRef(LuaEngine& engine);
    
    /**
     * @brief Get the compiled action reference for an engine
     * @param engine The LuaEngine
     * @return The reference ID, or -1 if not compiled
     */
    int getActionRef(LuaEngine& engine);

    // ========================================================================
    // Execution Helpers
    // ========================================================================
    
    /**
     * @brief Execute all child rules
     * @param engine The LuaEngine
     * @param context The execution context
     * @param parameters The parameters
     * @return Vector of child rule results
     */
    std::vector<RuleResult> executeChildRules(LuaEngine& engine, RuleContext& context,
                                               const std::vector<RuleParameter>& parameters);

    /**
     * @brief Evaluate the compiled expression
     * @param engine The LuaEngine
     * @param context The execution context
     * @param parameters The parameters
     * @return true if expression evaluates to truthy, false otherwise
     */
    bool evaluateExpression(LuaEngine& engine, RuleContext& context,
                            const std::vector<RuleParameter>& parameters);

    /**
     * @brief Execute the compiled action
     * @param engine The LuaEngine
     * @param context The execution context
     * @param parameters The parameters
     */
    void executeAction(LuaEngine& engine, RuleContext& context,
                       const std::vector<RuleParameter>& parameters);

    /**
     * @brief Set failure status on a RuleResult
     * @param result The result to modify
     * @param message The error message
     * @param wasCached Whether this was a cached result
     */
    static void setFailure(RuleResult& result, const std::string& message, 
                           bool wasCached = false) noexcept;
};

} // namespace fastrules
