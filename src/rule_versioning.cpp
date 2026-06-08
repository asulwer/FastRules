#include "fastrules/rule_versioning.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"
#include <sstream>
#include <iomanip>

namespace fastrules {

// ============================================================================
// RuleVersionHistory implementation
// ============================================================================

void RuleVersionHistory::addVersion(const RuleVersion& version) {
    std::lock_guard<std::mutex> lock(mutex_);
    versions_.push_back(version);
}

std::optional<RuleVersion> RuleVersionHistory::getLatest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (versions_.empty()) return std::nullopt;
    return versions_.back();
}

std::optional<RuleVersion> RuleVersionHistory::getVersion(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& v : versions_) {
        if (v.versionId == versionId) return v;
    }
    return std::nullopt;
}

std::optional<RuleVersion> RuleVersionHistory::getVersionAt(
    std::chrono::system_clock::time_point timestamp) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const RuleVersion* best = nullptr;
    for (const auto& v : versions_) {
        if (v.createdAt <= timestamp) {
            if (!best || v.createdAt > best->createdAt) {
                best = &v;
            }
        }
    }
    
    if (best) return *best;
    return std::nullopt;
}

std::vector<RuleVersion> RuleVersionHistory::listVersions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RuleVersion> result = versions_;
    std::reverse(result.begin(), result.end());
    return result;
}

Rule RuleVersionHistory::rollbackTo(const std::string& versionId) const {
    auto versionOpt = getVersion(versionId);
    if (!versionOpt.has_value()) {
        throw RuleException("Version not found: " + versionId);
    }
    
    const auto& version = versionOpt.value();
    Rule rule;
    rule.id = version.ruleId;
    rule.expression = version.expression;
    rule.action = version.action;
    rule.priority = version.priority;
    rule.isActive = version.isActive;
    
    return rule;
}

std::vector<std::string> RuleVersionHistory::diff(
    const std::string& fromVersionId,
    const std::string& toVersionId) const {
    
    auto fromOpt = getVersion(fromVersionId);
    auto toOpt = getVersion(toVersionId);
    
    std::vector<std::string> differences;
    
    if (!fromOpt.has_value()) {
        differences.push_back("Source version '" + fromVersionId + "' not found");
        return differences;
    }
    if (!toOpt.has_value()) {
        differences.push_back("Target version '" + toVersionId + "' not found");
        return differences;
    }
    
    const auto& from = fromOpt.value();
    const auto& to = toOpt.value();
    
    if (from.expression != to.expression) {
        differences.push_back("Expression changed");
    }
    if (from.action != to.action) {
        differences.push_back("Action changed");
    }
    if (from.priority != to.priority) {
        differences.push_back("Priority changed: " + std::to_string(from.priority) + 
                              " → " + std::to_string(to.priority));
    }
    if (from.isActive != to.isActive) {
        differences.push_back("Active state changed: " + std::string(from.isActive ? "true" : "false") +
                              " → " + std::string(to.isActive ? "true" : "false"));
    }
    
    return differences;
}

std::vector<RuleVersion> RuleVersionHistory::getVersions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return versions_;
}

// ============================================================================
// RuleVersionManager implementation
// ============================================================================

void RuleVersionManager::snapshotWorkflow(const Workflow& workflow, 
                                          const std::string& author,
                                          const std::string& summary) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& rule : workflow.rules) {
        snapshotRule(*rule, author, summary);
    }
}

void RuleVersionManager::snapshotRule(const Rule& rule, const std::string& author,
                                      const std::string& summary) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    RuleVersion version;
    version.versionId = generateVersionId();
    version.ruleId = rule.id;
    version.expression = rule.expression;
    version.action = rule.action;
    version.priority = rule.priority;
    version.isActive = rule.isActive;
    version.createdAt = std::chrono::system_clock::now();
    version.author = author;
    version.changeSummary = summary;
    
    auto& history = histories_[rule.id];
    auto latest = history.getLatest();
    if (latest.has_value()) {
        version.parentVersionId = latest->versionId;
    }
    
    history.ruleId = rule.id;
    history.addVersion(version);
}

std::optional<RuleVersionHistory> RuleVersionManager::getHistory(const std::string& ruleId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = histories_.find(ruleId);
    if (it == histories_.end()) return std::nullopt;
    RuleVersionHistory result = it->second;
    return result;
}

Workflow RuleVersionManager::rollbackWorkflow(const std::string& workflowId,
                                               const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Workflow workflow;
    workflow.id = workflowId;
    
    for (const auto& [ruleId, history] : histories_) {
        try {
            auto rule = history.rollbackTo(versionId);
            workflow.rules.push_back(std::make_shared<Rule>(rule));
        } catch (...) {
            // Rule didn't have that version, skip
        }
    }
    
    return workflow;
}

std::vector<std::string> RuleVersionManager::getTrackedRuleIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [id, _] : histories_) {
        result.push_back(id);
    }
    return result;
}

std::string RuleVersionManager::generateVersionId() const {
    static std::atomic<int> counter{0};
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H%M%S");
    ss << "-" << std::hex << (++counter);
    return ss.str();
}

} // namespace fastrules
