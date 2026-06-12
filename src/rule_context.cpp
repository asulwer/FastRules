#include "fastrules/rule_context.hpp"

#include <shared_mutex>

namespace fastrules {

void RuleContext::setResult(const std::string& ruleName, const RuleResult& result) {
    std::unique_lock lock(mutex_);
    results_[ruleName] = result;
}

std::optional<RuleResult> RuleContext::getResult(const std::string& ruleName) const {
    std::shared_lock lock(mutex_);
    auto it = results_.find(ruleName);
    if (it != results_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool RuleContext::hasResult(const std::string& ruleName) const {
    std::shared_lock lock(mutex_);
    return results_.contains(ruleName);
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
    variables_.clear();
}

void RuleContext::setLastError(const std::string& ruleName, const std::string& error) {
    std::unique_lock lock(mutex_);
    lastError_ = {ruleName, error};
}

std::optional<std::pair<std::string, std::string>> RuleContext::getLastError() const {
    std::shared_lock lock(mutex_);
    return lastError_;
}

void RuleContext::clearLastError() {
    std::unique_lock lock(mutex_);
    lastError_.reset();
}

} // namespace fastrules
