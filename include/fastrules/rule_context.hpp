#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include <any>
#include <shared_mutex>

#include "rule_result.hpp"

namespace fastrules {

class Rule;

// Thread-safe context for rule execution, storing intermediate results for dependency access
class RuleContext {
public:
    RuleContext() = default;

    // Store result of a rule execution (by ID and name)
    void setResult(int ruleId, const std::string& ruleName, const RuleResult& result);

    // Retrieve result of a previously executed rule (by ID)
    [[nodiscard]] std::optional<RuleResult> getResult(int ruleId) const;
    
    // Retrieve result of a previously executed rule (by name)
    [[nodiscard]] std::optional<RuleResult> getResult(const std::string& ruleName) const;

    // Check if a rule has been executed in this context (by ID or name)
    [[nodiscard]] bool hasResult(int ruleId) const;
    [[nodiscard]] bool hasResult(const std::string& ruleName) const;

    // Access to context-scoped variables (for Lua `context` global)
    void setVariable(const std::string& name, std::any value);
    [[nodiscard]] std::optional<std::any> getVariable(const std::string& name) const;

    // Reset for reuse
    void clear();

    // Last error tracking for better diagnostics
    void setLastError(int ruleId, const std::string& error);
    [[nodiscard]] std::optional<std::pair<int, std::string>> getLastError() const;
    void clearLastError();

private:
    std::unordered_map<int, RuleResult> results_;
    std::unordered_map<std::string, int> nameToId_;     // Maps rule names to IDs for lookup
    std::unordered_map<std::string, std::any> variables_;
    mutable std::shared_mutex mutex_;
    std::optional<std::pair<int, std::string>> lastError_; // {ruleId, errorMessage}
};

} // namespace fastrules
