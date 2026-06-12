#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace fastrules {

// Forward declarations
class Rule;
class Workflow;
class LuaEngine;

// Represents a snapshot of a rule at a point in time
struct RuleVersion {
    std::string versionId;       // e.g., "v1", "v2", "2024-06-05-a1b2"
    std::string ruleName;          // Human-readable rule name
    std::string expression;
    std::string action;
    int priority = 0;
    bool isActive = true;
    
    std::chrono::system_clock::time_point createdAt;
    std::string author;          // Who made this version
    std::string changeSummary;   // Brief description of changes
    std::string parentVersionId; // Previous version (for diff)
};

// Version history for a single rule
class RuleVersionHistory {
public:
    using Id = std::string;
    
    Id ruleId;
    std::string ruleName;
    
    // Add a new version to history
    void addVersion(const RuleVersion& version);
    
    // Get latest version
    [[nodiscard]] std::optional<RuleVersion> getLatest() const;
    
    // Get specific version by ID
    [[nodiscard]] std::optional<RuleVersion> getVersion(const std::string& versionId) const;
    
    // Get version at a specific point in time
    [[nodiscard]] std::optional<RuleVersion> getVersionAt(
        std::chrono::system_clock::time_point timestamp) const;
    
    // List all versions (newest first)
    [[nodiscard]] std::vector<RuleVersion> listVersions() const;
    
    // Rollback: restore rule to a previous version
    [[nodiscard]] Rule rollbackTo(const std::string& versionId) const;
    
    // Compare two versions and return differences
    [[nodiscard]] std::vector<std::string> diff(
        const std::string& fromVersionId,
        const std::string& toVersionId) const;
    
    // Serialization (implemented by extensions)
    // Core provides data access; JSON/XML/DB extensions handle formatting
    [[nodiscard]] std::vector<RuleVersion> getVersions() const;
    
    RuleVersionHistory() = default;
    RuleVersionHistory(const RuleVersionHistory& other) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        ruleId = other.ruleId;
        ruleName = other.ruleName;
        versions_ = other.versions_;
    }
    RuleVersionHistory& operator=(const RuleVersionHistory& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(other.mutex_);
            std::lock_guard<std::mutex> lock2(mutex_);
            ruleId = other.ruleId;
            ruleName = other.ruleName;
            versions_ = other.versions_;
        }
        return *this;
    }
    RuleVersionHistory(RuleVersionHistory&&) = default;
    RuleVersionHistory& operator=(RuleVersionHistory&&) = default;
    
private:
    std::vector<RuleVersion> versions_;
    mutable std::mutex mutex_;
};

// Version manager: holds history for all rules in a workflow
class RuleVersionManager {
public:
    // Snapshot current workflow state as versions
    void snapshotWorkflow(const Workflow& workflow, const std::string& author = "", 
                          const std::string& summary = "");
    
    // Snapshot a single rule
    void snapshotRule(const Rule& rule, const std::string& author = "",
                      const std::string& summary = "");
    
    // Get history for a rule
    [[nodiscard]] std::optional<RuleVersionHistory> getHistory(int ruleId) const;
    
    // Rollback entire workflow to a snapshot
    [[nodiscard]] Workflow rollbackWorkflow(int workflowId,
                                             const std::string& versionId) const;
    
    // List all tracked rule IDs
    [[nodiscard]] std::vector<std::string> getTrackedRuleIds() const;
    
    // Auto-snapshot on rule changes
    void enableAutoSnapshot(bool enabled) { autoSnapshot_ = enabled; }
    [[nodiscard]] bool isAutoSnapshotEnabled() const { return autoSnapshot_; }
    
private:
    std::unordered_map<std::string, RuleVersionHistory> histories_;
    mutable std::mutex mutex_;
    bool autoSnapshot_ = false;
    
    [[nodiscard]] std::string generateVersionId() const;
};

} // namespace fastrules
