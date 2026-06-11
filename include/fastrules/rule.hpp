#pragma once

#include <string>
#include <optional>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <any>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>
#include <mutex>

#include "rule_result.hpp"

namespace fastrules {

// Forward declarations
class RuleContext;
class RateLimiter;
class LuaEngine;
class AsyncWorkflow;
struct AsyncRuleResult;
class AsyncWorkflowTask;

struct RuleParameter {
    std::string name;
    std::optional<std::type_index> type;  // set for registered types; nullopt for primitives
    std::any value;

    RuleParameter() = default;

    // Template constructor — type inferred from value
    template<typename T>
    RuleParameter(std::string n, T v)
        : name(std::move(n))
        , type(std::type_index(typeid(T)))
        , value(std::move(v)) {}

    // Constructor for primitive types (string type tag + value)
    template<typename T>
    RuleParameter(std::string n, std::string typeTag, T v)
        : name(std::move(n))
        , value(std::move(v)) {
        if (typeTag == "int") type = std::type_index(typeid(int));
        else if (typeTag == "double") type = std::type_index(typeid(double));
        else if (typeTag == "bool") type = std::type_index(typeid(bool));
        else if (typeTag == "string") type = std::type_index(typeid(std::string));
        else type = std::type_index(typeid(T));
    }

    RuleParameter(RuleParameter&&) noexcept = default;
    RuleParameter& operator=(RuleParameter&&) noexcept = default;
    RuleParameter(const RuleParameter&) = default;
    RuleParameter& operator=(const RuleParameter&) = default;
};

class Rule {
public:
    using Id = int;

    Rule() = default;
    ~Rule() = default;
    Rule(const Rule&) = delete;
    Rule(Rule&&) noexcept = default;
    Rule& operator=(const Rule&) = delete;
    Rule& operator=(Rule&&) noexcept = default;

    // --- Public fields ---

    Id id = 0;
    std::string name;           // Human-readable name for referencing rules
    std::string description;
    bool isActive = true;
    int priority = 0;

    std::string expression;
    std::string action;

    std::optional<std::string> dependsOnRuleName;  // Reference by name instead of ID
    std::vector<std::shared_ptr<Rule>> childRules;
    std::weak_ptr<Rule> parentRule;

    std::optional<std::chrono::milliseconds> timeout;
    std::optional<std::chrono::milliseconds> cacheDuration;

    std::optional<std::shared_ptr<RateLimiter>> rateLimiter;

    // --- Cache invalidation ---
    // Call this after modifying rule properties (expression, action, etc.)
    // to invalidate cached results. Returns the new generation number.
    int invalidateCache();

    // --- Validated setters ---
    void setTimeout(std::optional<std::chrono::milliseconds> value) {
        if (value.has_value() && value->count() <= 0)
            throw std::invalid_argument("timeout must be > 0 if set");
        timeout = std::move(value);
    }

    void setCacheDuration(std::optional<std::chrono::milliseconds> value) {
        cacheDuration = std::move(value);
    }

    void setPriority(int value) {
        if (value < 0) throw std::invalid_argument("priority must be >= 0");
        priority = value;
    }

    // --- Lifecycle (friend access for Workflow/AsyncWorkflow) ---

    [[nodiscard]] bool getIsCompiled() const noexcept { return isCompiled; }
    [[nodiscard]] bool getIsValidated() const noexcept { return isValidated; }

    friend class Workflow;
    friend class AsyncWorkflow;
    friend class RuleAwaitable;
    friend struct AsyncRuleResult;
    friend struct AsyncRulePromise;
    friend class AsyncWorkflowTask;

    // Compilation
    void compile(class LuaEngine& engine);

    // Validation
    void validate(const std::vector<std::reference_wrapper<const Rule>>& allRules);

    // Circular dependency detection
    [[nodiscard]] bool hasCircularDependency(const std::vector<std::reference_wrapper<const Rule>>& allRules) const;
    [[nodiscard]] std::vector<Id> getDependencyChain(const std::vector<std::reference_wrapper<const Rule>>& allRules) const;

    // Execution (typically through Workflow, but also available for direct use)
    RuleResult execute(class LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters);

    // Static factories
    static Rule isNotNull(const std::string& parameterName, const std::string& description = "");
    static Rule greaterThan(const std::string& parameterName, double value, const std::string& description = "");
    static Rule lessThan(const std::string& parameterName, double value, const std::string& description = "");
    static Rule equals(const std::string& parameterName, const std::string& value, const std::string& description = "");
    static Rule matchesRegex(const std::string& parameterName, const std::string& pattern, const std::string& description = "");
    static Rule contains(const std::string& parameterName, const std::string& substring, const std::string& description = "");

    // Builder
    class Builder {
    public:
        explicit Builder(int ruleId) : rule_(std::make_shared<Rule>()) {
            rule_->id = ruleId;
        }

        Builder& withDescription(const std::string& desc) {
            rule_->description = desc;
            return *this;
        }
        Builder& withExpression(const std::string& expr) {
            rule_->expression = expr;
            return *this;
        }
        Builder& withAction(const std::string& act) {
            rule_->action = act;
            return *this;
        }
        Builder& withTimeout(std::chrono::milliseconds ms) {
            rule_->timeout = ms;
            return *this;
        }
        Builder& withCacheDuration(std::chrono::milliseconds ms) {
            rule_->cacheDuration = ms;
            return *this;
        }
        Builder& withPriority(int p) {
            rule_->priority = p;
            return *this;
        }
        Builder& dependsOn(const Id& ruleId) {
            rule_->dependsOnRuleId = ruleId;
            return *this;
        }
        Builder& dependsOn(const Id& ruleId, const std::vector<std::reference_wrapper<const Rule>>& allRules) {
            rule_->dependsOnRuleId = ruleId;
            // Temporarily add this rule to allRules for validation
            std::vector<std::reference_wrapper<const Rule>> allWithThis = allRules;
            allWithThis.emplace_back(*rule_);
            rule_->validate(allWithThis);
            return *this;
        }
        Builder& withChild(std::shared_ptr<Rule> child) {
            child->parentRule = rule_;
            rule_->childRules.push_back(std::move(child));
            return *this;
        }
        Builder& withRateLimiter(std::shared_ptr<RateLimiter> limiter) {
            rule_->rateLimiter = std::move(limiter);
            return *this;
        }
        Builder& active(bool a = true) {
            rule_->isActive = a;
            return *this;
        }

        [[nodiscard]] std::shared_ptr<Rule> build() {
            return std::move(rule_);
        }

        [[nodiscard]] std::shared_ptr<Rule> buildAndValidate(const std::vector<std::reference_wrapper<const Rule>>& allRules) {
            rule_->validate(allRules);
            return std::move(rule_);
        }

    private:
        std::shared_ptr<Rule> rule_;
    };

    static Builder create(const Id& id, const std::string& expression, bool active = true);

private:
    bool isCompiled = false;
    bool isValidated = false;

    // Cache generation - incremented on modifications to invalidate stale cache
    mutable int cacheGeneration_ = 0;
    mutable std::unique_ptr<std::mutex> cacheMutex_ = std::make_unique<std::mutex>();

    // Compiled refs cached by compile() - per-engine for parallel execution support
    std::optional<int> compiledExpressionRef;
    std::optional<int> compiledActionRef;
    std::unordered_map<const LuaEngine*, int> engineExpressionRefs_;
    std::unordered_map<const LuaEngine*, int> engineActionRefs_;

    struct CacheEntry {
        std::shared_ptr<RuleResult> result;
        std::chrono::steady_clock::time_point expiresAt;
        int generation;  // Store generation at time of caching
    };
    mutable std::unordered_map<std::string, CacheEntry> cache_;

    std::string buildCacheKey(const std::vector<RuleParameter>& parameters) const;
    void storeInCache(const std::vector<RuleParameter>& parameters, const RuleResult& result) const;
    void setFailure(RuleResult& result, const std::string& message, bool wasCached = false) noexcept;

    std::vector<RuleResult> executeChildRules(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters);
    bool evaluateExpression(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters);
    void executeAction(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters);
    
    // Get or compile expression ref for a specific engine
    int getExpressionRef(LuaEngine& engine);
    // Get or compile action ref for a specific engine
    int getActionRef(LuaEngine& engine);
};

} // namespace fastrules
