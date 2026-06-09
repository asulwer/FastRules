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

    // Store result of a rule execution
    void setResult(int ruleId, const RuleResult& result);

    // Retrieve result of a previously executed rule
    [[nodiscard]] std::optional<RuleResult> getResult(int ruleId) const;

    // Check if a rule has been executed in this context
    [[nodiscard]] bool hasResult(int ruleId) const;

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
    std::unordered_map<std::string, std::any> variables_;
    mutable std::shared_mutex mutex_;
    std::optional<std::pair<int, std::string>> lastError_; // {ruleId, errorMessage}
};

} // namespace fastrules
