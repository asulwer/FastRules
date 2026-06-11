#include "fastrules/rule_context.hpp"

#include <shared_mutex>

namespace fastrules {

void RuleContext::setResult(int ruleId, const std::string& ruleName, const RuleResult& result) {
    std::unique_lock lock(mutex_);
    results_[ruleId] = result;
    if (!ruleName.empty()) {
        nameToId_[ruleName] = ruleId;
    }
}

std::optional<RuleResult> RuleContext::getResult(int ruleId) const {
    std::shared_lock lock(mutex_);
    auto it = results_.find(ruleId);
    if (it != results_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<RuleResult> RuleContext::getResult(const std::string& ruleName) const {
    std::shared_lock lock(mutex_);
    auto nameIt = nameToId_.find(ruleName);
    if (nameIt != nameToId_.end()) {
        auto resultIt = results_.find(nameIt->second);
        if (resultIt != results_.end()) {
            return resultIt->second;
        }
    }
    return std::nullopt;
}

bool RuleContext::hasResult(int ruleId) const {
    std::shared_lock lock(mutex_);
    return results_.contains(ruleId);
}

bool RuleContext::hasResult(const std::string& ruleName) const {
    std::shared_lock lock(mutex_);
    auto nameIt = nameToId_.find(ruleName);
    if (nameIt != nameToId_.end()) {
        return results_.contains(nameIt->second);
    }
    return false;
}

void RuleContext::setVariable(const std::string& name, std::any value) {
    std::unique_lock lock(mutex_);
    variables_[name] = std::move(value);
}

std::optional<std::any> RuleContext::getVariable(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void RuleContext::clear() {
    std::unique_lock lock(mutex_);
    results_.clear();
    nameToId_.clear();
    variables_.clear();
    lastError_.reset();
}

void RuleContext::setLastError(int ruleId, const std::string& error) {
    std::unique_lock lock(mutex_);
    lastError_ = {ruleId, error};
}

std::optional<std::pair<int, std::string>> RuleContext::getLastError() const {
    std::shared_lock lock(mutex_);
    return lastError_;
}

void RuleContext::clearLastError() {
    std::unique_lock lock(mutex_);
    lastError_.reset();
}

} // namespace fastrules
