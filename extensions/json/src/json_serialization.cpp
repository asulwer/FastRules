#include <fastrules/json_serialization.hpp>
#include <fastrules/json_loader.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace fastrules {
namespace ext {

using json = nlohmann::json;

// ============================================================================
// RuleVersionHistory
// ============================================================================

std::string JsonSerialization::serialize(const RuleVersionHistory& history) {
    json j;
    j["ruleId"] = history.ruleName;
    j["ruleName"] = history.ruleName;
    
    json versions = json::array();
    for (const auto& v : history.getVersions()) {
        json ver;
        ver["versionId"] = v.versionId;
        ver["ruleId"] = v.ruleName;
        ver["expression"] = v.expression;
        ver["action"] = v.action;
        ver["priority"] = v.priority;
        ver["isActive"] = v.isActive;
        
        auto time_t = std::chrono::system_clock::to_time_t(v.createdAt);
        std::stringstream ss;
#ifdef _WIN32
        std::tm tm_buf{};
        gmtime_s(&tm_buf, &time_t);
        ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
#else
        std::tm tm_buf{};
        gmtime_r(&time_t, &tm_buf);
        ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
#endif
        ver["createdAt"] = ss.str();
        
        ver["author"] = v.author;
        ver["changeSummary"] = v.changeSummary;
        ver["parentVersionId"] = v.parentVersionId;
        versions.push_back(ver);
    }
    j["versions"] = versions;
    
    return j.dump(2);
}

std::optional<RuleVersionHistory> JsonSerialization::deserializeRuleVersionHistory(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        RuleVersionHistory history;
        history.ruleName = j.value("ruleId", "");
        history.ruleName = j.value("ruleName", "");
        
        for (const auto& ver : j["versions"]) {
            RuleVersion v;
            v.versionId = ver.value("versionId", "");
            v.ruleName = ver.value("ruleId", "");
            v.expression = ver.value("expression", "");
            v.action = ver.value("action", "");
            
            v.priority = ver.value("priority", 0);
            v.isActive = ver.value("isActive", true);
            
            if (ver.contains("createdAt") && ver["createdAt"].is_string()) {
                std::string timeStr = ver["createdAt"];
                std::tm tm = {};
                std::stringstream ss(timeStr);
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                v.createdAt = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            } else {
                v.createdAt = std::chrono::system_clock::now();
            }
            
            v.author = ver.value("author", "");
            v.changeSummary = ver.value("changeSummary", "");
            v.parentVersionId = ver.value("parentVersionId", "");
            
            history.addVersion(v);
        }
        
        return history;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// ============================================================================
// RuleVersionManager
// ============================================================================

std::string JsonSerialization::serialize(const RuleVersionManager& manager) {
    json j;
    j["exportDate"] = []() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
#ifdef _WIN32
        std::tm tm;
        if (gmtime_s(&tm, &time_t) != 0) {
            throw std::runtime_error("gmtime_s failed");
        }
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
#else
        std::tm tm;
        if (gmtime_r(&time_t, &tm) == nullptr) {
            throw std::runtime_error("gmtime_r failed");
        }
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
#endif
        return ss.str();
    }();
    
    json histories = json::array();
    for (const auto& ruleId : manager.getTrackedRuleIds()) {
        auto historyOpt = manager.getHistory(std::stoi(ruleId));
        if (historyOpt) {
            histories.push_back(json::parse(serialize(historyOpt.value())));
        }
    }
    j["histories"] = histories;
    
    return j.dump(2);
}

void JsonSerialization::deserialize(RuleVersionManager& manager, const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        for (const auto& hist : j["histories"]) {
            auto historyOpt = deserializeRuleVersionHistory(hist.dump());
            if (historyOpt) {
                for (const auto& ver : historyOpt->getVersions()) {
                    Rule rule;
                    rule.id = std::stoi(ver.ruleName);
                    rule.expression = ver.expression;
                    rule.action = ver.action;
                    rule.isActive = ver.isActive;
                    manager.snapshotRule(rule, ver.author, ver.changeSummary);
                }
            }
        }
    } catch (const std::exception&) {
        // Import failed
    }
}

// ============================================================================
// ExecutionTrace
// ============================================================================

std::string JsonSerialization::serialize(const ExecutionTrace& trace) {
    json j;
    j["workflowId"] = trace.workflowId;
    j["overallSuccess"] = trace.overallSuccess;
    j["totalDurationNs"] = trace.totalDuration().count();

    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(trace.totalDuration());
    j["totalDurationMs"] = durationMs.count();

    json stepsArray = json::array();
    for (const auto& step : trace.steps) {
        json stepJson;
        stepJson["ruleId"] = step.ruleName;
        stepJson["stage"] = step.stage;
        stepJson["success"] = step.success;
        stepJson["durationNs"] = step.duration().count();
        auto stepMs = std::chrono::duration_cast<std::chrono::milliseconds>(step.duration());
        stepJson["durationMs"] = stepMs.count();
        if (step.message) stepJson["message"] = *step.message;
        if (step.dependencyId) stepJson["dependencyId"] = *step.dependencyId;
        if (step.expression) stepJson["expression"] = *step.expression;
        if (step.action) stepJson["action"] = *step.action;
        stepsArray.push_back(stepJson);
    }
    j["steps"] = stepsArray;
    j["stepCount"] = trace.steps.size();

    return j.dump(2);
}

std::optional<ExecutionTrace> JsonSerialization::deserializeExecutionTrace(const std::string& /*json*/) {
    // Not typically needed — traces are export-only
    return std::nullopt;
}

// ============================================================================
// PerformanceCounters
// ============================================================================

std::string JsonSerialization::serialize(const PerformanceCounters& counters) {
    auto c = counters.getCounters();
    json j;
    j["totalRulesExecuted"] = c.totalRulesExecuted.load();
    j["totalRulesSuccessful"] = c.totalRulesSuccessful.load();
    j["totalRulesFailed"] = c.totalRulesFailed.load();
    j["totalRulesSkipped"] = c.totalRulesSkipped.load();
    j["totalRulesCached"] = c.totalRulesCached.load();
    j["totalRulesTimedOut"] = c.totalRulesTimedOut.load();
    j["totalRulesRateLimited"] = c.totalRulesRateLimited.load();
    j["totalCompileCount"] = c.totalCompileCount.load();
    j["totalCompileFailures"] = c.totalCompileFailures.load();
    j["totalExecutionTimeNs"] = c.totalExecutionTimeNs.load();
    j["averageExecutionTimeMs"] = counters.getAverageExecutionTimeMs();
    j["successRate"] = counters.getSuccessRate();
    j["cacheHitRate"] = counters.getCacheHitRate();

    return j.dump(2);
}

// ============================================================================
// Workflow / Rule — delegates to JsonLoader
// ============================================================================

std::string JsonSerialization::serialize(const Workflow& workflow) {
    return JsonLoader::saveWorkflow(workflow);
}

Workflow JsonSerialization::deserializeWorkflow(const std::string& json) {
    return JsonLoader::loadWorkflow(json);
}

std::string JsonSerialization::serialize(const Rule& rule) {
    return JsonLoader::saveRule(rule);
}

std::shared_ptr<Rule> JsonSerialization::deserializeRule(const std::string& json) {
    return JsonLoader::loadRule(json);
}

} // namespace ext
} // namespace fastrules
