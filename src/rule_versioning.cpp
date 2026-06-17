/**
 * @file rule_versioning.cpp
 * @brief Rule versioning and history tracking
 * 
 * This file implements version control for business rules:
 * - RuleVersionHistory: Stores versions of a single rule
 * - RuleVersionManager: Manages histories for all rules
 * 
 * Version Control Features:
 * - Automatic snapshots of rule state
 * - Point-in-time restoration
 * - Diff between versions
 * - Parent-child version relationships
 * 
 * Use Cases:
 * - Audit trails for compliance
 * - Rollback to previous rule versions
 * - Review changes before deployment
 * - Track who changed what and when
 * 
 * Thread Safety:
 * - All operations are mutex-protected
 * - Safe for concurrent snapshot and query
 * 
 * Version ID Format:
 * - YYYYMMDD-HHMMSS-hex_counter
 * - Example: 20240518-143022-a1b2c
 * 
 * Limitations:
 * - In-memory storage only (no persistence)
 * - No garbage collection of old versions
 * - All versions kept indefinitely
 */

#include "fastrules/rule_versioning.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"
#include <sstream>
#include <iomanip>

namespace fastrules {

// ============================================================================
// RuleVersionHistory implementation
// ============================================================================

/**
 * @brief Add a new version to the history
 * 
 * Versions are stored chronologically. The latest version
 * is always at the end of the vector.
 * 
 * @param version The RuleVersion to add
 */
void RuleVersionHistory::addVersion(const RuleVersion& version) {
    std::lock_guard<std::mutex> lock(mutex_);
    versions_.push_back(version);
}

/**
 * @brief Get the latest version
 * @return Optional containing latest version, or nullopt if empty
 */
std::optional<RuleVersion> RuleVersionHistory::getLatest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (versions_.empty()) return std::nullopt;
    return versions_.back();
}

/**
 * @brief Get a specific version by ID
 * 
 * Searches linearly through version history.
 * For large histories, consider indexing by versionId.
 * 
 * @param versionId The version ID to look up
 * @return Optional containing the version, or nullopt if not found
 */
std::optional<RuleVersion> RuleVersionHistory::getVersion(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& v : versions_) {
        if (v.versionId == versionId) return v;
    }
    return std::nullopt;
}

/**
 * @brief Get the version active at a specific time
 * 
 * Finds the newest version created at or before the timestamp.
 * Useful for "what did the rule look like on Tuesday?"
 * 
 * @param timestamp Point in time to query
 * @return Optional containing the version, or nullopt if no versions existed
 */
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

/**
 * @brief List all versions (newest first)
 * @return Vector of versions in reverse chronological order
 */
std::vector<RuleVersion> RuleVersionHistory::listVersions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RuleVersion> result = versions_;
    std::reverse(result.begin(), result.end());
    return result;
}

/**
 * @brief Rollback to a specific version
 * 
 * Creates a new Rule configured from the stored version.
 * Note: The ID is parsed from ruleName (which was stored as string).
 * 
 * @param versionId Version ID to rollback to
 * @return New Rule instance with that version's configuration
 * @throws RuleException if version not found
 */
Rule RuleVersionHistory::rollbackTo(const std::string& versionId) const {
    auto versionOpt = getVersion(versionId);
    if (!versionOpt.has_value()) {
        throw RuleException("Version not found: " + versionId);
    }
    
    const auto& version = versionOpt.value();
    Rule rule;
    rule.id = std::stoi(version.ruleName);
    rule.expression = version.expression;
    rule.action = version.action;
    rule.priority = version.priority;
    rule.isActive = version.isActive;
    
    return rule;
}

/**
 * @brief Compare two versions and list differences
 * 
 * Checks: expression, action, priority, isActive
 * Does NOT compare: metadata (author, timestamp, etc.)
 * 
 * @param fromVersionId Source version
 * @param toVersionId Target version
 * @return Vector of human-readable difference descriptions
 */
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
                              " -> " + std::to_string(to.priority));
    }
    if (from.isActive != to.isActive) {
        differences.push_back("Active state changed: " + std::string(from.isActive ? "true" : "false") +
                              " -> " + std::string(to.isActive ? "true" : "false"));
    }
    
    return differences;
}

/**
 * @brief Get all versions in storage order
 * @return Vector of versions (oldest first)
 */
std::vector<RuleVersion> RuleVersionHistory::getVersions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return versions_;
}

// ============================================================================
// RuleVersionManager implementation
// ============================================================================

/**
 * @brief Snapshot all rules in a workflow
 * 
 * Convenience method to version every rule in a workflow
 * with the same author and change summary.
 * 
 * @param workflow Workflow to snapshot
 * @param author Who made the changes
 * @param summary Brief description of changes
 */
void RuleVersionManager::snapshotWorkflow(const Workflow& workflow, 
                                          const std::string& author,
                                          const std::string& summary) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& rule : workflow.rules) {
        snapshotRule(*rule, author, summary);
    }
}

/**
 * @brief Snapshot a single rule
 * 
 * Creates a new RuleVersion from current rule state and
 * stores it in the rule's history. Links to parent version
 * for ancestry tracking.
 * 
 * @param rule Rule to snapshot
 * @param author Who made the changes
 * @param summary Brief description of changes
 */
void RuleVersionManager::snapshotRule(const Rule& rule, const std::string& author,
                                      const std::string& summary) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    RuleVersion version;
    version.versionId = generateVersionId();
    version.ruleName = std::to_string(rule.id);
    version.expression = rule.expression;
    version.action = rule.action;
    version.priority = rule.priority;
    version.isActive = rule.isActive;
    version.createdAt = std::chrono::system_clock::now();
    version.author = author;
    version.changeSummary = summary;
    
    auto& history = histories_[std::to_string(rule.id)];
    auto latest = history.getLatest();
    if (latest.has_value()) {
        version.parentVersionId = latest->versionId;
    }
    
    history.ruleName = std::to_string(rule.id);
    history.addVersion(version);
}

/**
 * @brief Get version history for a rule
 * @param ruleId ID of the rule
 * @return Optional containing the history, or nullopt if not tracked
 */
std::optional<RuleVersionHistory> RuleVersionManager::getHistory(int ruleId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = histories_.find(std::to_string(ruleId));
    if (it == histories_.end()) return std::nullopt;
    RuleVersionHistory result = it->second;
    return result;
}

/**
 * @brief Rollback entire workflow to a version
 * 
 * Creates a new Workflow with all rules rolled back to
 * the specified version. Rules without that version are skipped.
 * 
 * @param workflowId ID for the new workflow
 * @param versionId Version ID to rollback to
 * @return New workflow with rolled-back rules
 */
Workflow RuleVersionManager::rollbackWorkflow(int workflowId,
                                               const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Workflow workflow;
    workflow.id = workflowId;
    
    for (const auto& [ruleId, history] : histories_) {
        try {
            auto rule = history.rollbackTo(versionId);
            workflow.rules.push_back(std::make_shared<Rule>(std::move(rule)));
        } catch (...) {
            // Rule didn't have that version, skip
        }
    }
    
    return std::move(workflow);
}

/**
 * @brief Get list of all tracked rule IDs
 * @return Vector of rule IDs as strings
 */
std::vector<std::string> RuleVersionManager::getTrackedRuleIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [id, _] : histories_) {
        result.push_back(id);
    }
    return result;
}

/**
 * @brief Generate unique version ID
 * 
 * Format: YYYYMMDD-HHMMSS-hex_counter
 * - Date and time provide human readability
 * - Counter ensures uniqueness within same second
 * - Atomic counter is process-local (not globally unique)
 * 
 * @return New unique version ID string
 */
std::string RuleVersionManager::generateVersionId() const {
    static std::atomic<int> counter{0};
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
#ifdef _WIN32
    std::tm tm_buf{};
    gmtime_s(&tm_buf, &time_t);
    ss << std::put_time(&tm_buf, "%Y%m%d-%H%M%S");
#else
    std::tm tm_buf{};
    gmtime_r(&time_t, &tm_buf);
    ss << std::put_time(&tm_buf, "%Y%m%d-%H%M%S");
#endif
    ss << "-" << std::hex << (++counter);
    return ss.str();
}

} // namespace fastrules
