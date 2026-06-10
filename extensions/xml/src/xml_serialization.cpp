#include <fastrules/xml_serialization.hpp>
#include <fastrules/xml_loader.hpp>
#include <pugixml.hpp>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace fastrules {
namespace ext {

// ============================================================================
// Helper: time point to/from ISO-8601 string
// ============================================================================

static std::string timePointToString(std::chrono::system_clock::time_point tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
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
    return ss.str();
}

static std::chrono::system_clock::time_point parseIsoTime(const std::string& str) {
    std::tm tm = {};
    std::stringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// ============================================================================
// Helper: append text child
// ============================================================================

static pugi::xml_node appendTextChild(pugi::xml_node& parent, const char* name, const std::string& value) {
    auto child = parent.append_child(name);
    child.text().set(value.c_str());
    return child;
}

static pugi::xml_node appendTextChild(pugi::xml_node& parent, const char* name, bool value) {
    auto child = parent.append_child(name);
    child.text().set(value);
    return child;
}

static pugi::xml_node appendTextChild(pugi::xml_node& parent, const char* name, int value) {
    auto child = parent.append_child(name);
    child.text().set(value);
    return child;
}

static pugi::xml_node appendTextChild(pugi::xml_node& parent, const char* name, long long value) {
    auto child = parent.append_child(name);
    child.text().set(value);
    return child;
}

static pugi::xml_node appendTextChild(pugi::xml_node& parent, const char* name, double value) {
    auto child = parent.append_child(name);
    child.text().set(value);
    return child;
}

// ============================================================================
// RuleVersionHistory
// ============================================================================

std::string XmlSerialization::serialize(const RuleVersionHistory& history) {
    pugi::xml_document doc;
    auto root = doc.append_child("ruleVersionHistory");
    root.append_attribute("ruleId").set_value(history.ruleId.c_str());
    root.append_attribute("ruleName").set_value(history.ruleName.c_str());

    auto versionsNode = root.append_child("versions");
    for (const auto& v : history.getVersions()) {
        auto ver = versionsNode.append_child("version");
        appendTextChild(ver, "versionId", v.versionId);
        appendTextChild(ver, "ruleId", v.ruleId);
        appendTextChild(ver, "expression", v.expression);
        appendTextChild(ver, "action", v.action);
        appendTextChild(ver, "priority", v.priority);
        appendTextChild(ver, "isActive", v.isActive);
        appendTextChild(ver, "createdAt", timePointToString(v.createdAt));
        appendTextChild(ver, "author", v.author);
        appendTextChild(ver, "changeSummary", v.changeSummary);
        appendTextChild(ver, "parentVersionId", v.parentVersionId);
    }

    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

std::optional<RuleVersionHistory> XmlSerialization::deserializeRuleVersionHistory(const std::string& xmlStr) {
    try {
        pugi::xml_document doc;
        auto result = doc.load_string(xmlStr.c_str());
        if (!result) return std::nullopt;

        auto root = doc.child("ruleVersionHistory");
        if (!root) return std::nullopt;

        RuleVersionHistory history;
        history.ruleId = root.attribute("ruleId").as_string("");
        history.ruleName = root.attribute("ruleName").as_string("");

        for (auto ver : root.child("versions").children("version")) {
            RuleVersion v;
            v.versionId = ver.child("versionId").text().as_string("");
            v.ruleId = ver.child("ruleId").text().as_string("");
            v.expression = ver.child("expression").text().as_string("");
            v.action = ver.child("action").text().as_string("");
            v.priority = ver.child("priority").text().as_int(0);
            v.isActive = ver.child("isActive").text().as_bool(true);
            v.author = ver.child("author").text().as_string("");
            v.changeSummary = ver.child("changeSummary").text().as_string("");
            v.parentVersionId = ver.child("parentVersionId").text().as_string("");

            auto createdAtStr = ver.child("createdAt").text().as_string("");
            if (createdAtStr != nullptr && createdAtStr[0] != '\0') {
                v.createdAt = parseIsoTime(createdAtStr);
            } else {
                v.createdAt = std::chrono::system_clock::now();
            }

            history.addVersion(v);
        }

        return history;
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================================
// RuleVersionManager
// ============================================================================

std::string XmlSerialization::serialize(const RuleVersionManager& manager) {
    pugi::xml_document doc;
    auto root = doc.append_child("ruleVersionManager");
    root.append_attribute("exportDate").set_value(timePointToString(std::chrono::system_clock::now()).c_str());

    auto historiesNode = root.append_child("histories");
    for (const auto& ruleId : manager.getTrackedRuleIds()) {
        auto histOpt = manager.getHistory(std::stoi(ruleId));
        if (histOpt) {
            auto histNode = historiesNode.append_child("history");
            // Embed the serialized history as XML
            std::string histXml = serialize(histOpt.value());
            // Parse and append
            pugi::xml_document tempDoc;
            tempDoc.load_string(histXml.c_str());
            if (auto inner = tempDoc.child("ruleVersionHistory")) {
                for (auto attr = inner.first_attribute(); attr; attr = attr.next_attribute()) {
                    histNode.append_attribute(attr.name()).set_value(attr.value());
                }
                for (auto child = inner.first_child(); child; child = child.next_sibling()) {
                    histNode.append_copy(child);
                }
            }
        }
    }

    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

void XmlSerialization::deserialize(RuleVersionManager& manager, const std::string& xmlStr) {
    try {
        pugi::xml_document doc;
        auto result = doc.load_string(xmlStr.c_str());
        if (!result) return;

        auto root = doc.child("ruleVersionManager");
        if (!root) return;

        for (auto histNode : root.child("histories").children("history")) {
            // Reconstruct inner XML for deserializeRuleVersionHistory
            pugi::xml_document innerDoc;
            auto innerRoot = innerDoc.append_child("ruleVersionHistory");
            innerRoot.append_attribute("ruleId").set_value(histNode.attribute("ruleId").as_string(""));
            innerRoot.append_attribute("ruleName").set_value(histNode.attribute("ruleName").as_string(""));
            for (auto child = histNode.first_child(); child; child = child.next_sibling()) {
                innerRoot.append_copy(child);
            }

            std::ostringstream innerOss;
            innerDoc.save(innerOss);
            auto historyOpt = deserializeRuleVersionHistory(innerOss.str());
            if (historyOpt) {
                for (const auto& ver : historyOpt->getVersions()) {
                    Rule rule;
                    rule.id = std::stoi(ver.ruleId);
                    rule.expression = ver.expression;
                    rule.action = ver.action;
                    rule.isActive = ver.isActive;
                    manager.snapshotRule(rule, ver.author, ver.changeSummary);
                }
            }
        }
    } catch (...) {
        // Import failed
    }
}

// ============================================================================
// ExecutionTrace
// ============================================================================

std::string XmlSerialization::serialize(const ExecutionTrace& trace) {
    pugi::xml_document doc;
    auto root = doc.append_child("executionTrace");
    root.append_attribute("workflowId").set_value(trace.workflowId);
    root.append_attribute("overallSuccess").set_value(trace.overallSuccess);

    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(trace.totalDuration());
    appendTextChild(root, "totalDurationMs", static_cast<int>(totalMs.count()));
    appendTextChild(root, "totalDurationNs", static_cast<long long>(trace.totalDuration().count()));
    appendTextChild(root, "stepCount", static_cast<int>(trace.steps.size()));

    auto stepsNode = root.append_child("steps");
    for (const auto& step : trace.steps) {
        auto stepNode = stepsNode.append_child("step");
        appendTextChild(stepNode, "ruleId", step.ruleId);
        appendTextChild(stepNode, "stage", step.stage);
        appendTextChild(stepNode, "success", step.success);

        auto stepMs = std::chrono::duration_cast<std::chrono::milliseconds>(step.duration());
        appendTextChild(stepNode, "durationMs", static_cast<int>(stepMs.count()));
        appendTextChild(stepNode, "durationNs", static_cast<long long>(step.duration().count()));

        if (step.message) appendTextChild(stepNode, "message", *step.message);
        if (step.dependencyId) appendTextChild(stepNode, "dependencyId", *step.dependencyId);
        if (step.expression) appendTextChild(stepNode, "expression", *step.expression);
        if (step.action) appendTextChild(stepNode, "action", *step.action);
    }

    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

// ============================================================================
// PerformanceCounters
// ============================================================================

std::string XmlSerialization::serialize(const PerformanceCounters& counters) {
    auto c = counters.getCounters();
    pugi::xml_document doc;
    auto root = doc.append_child("performanceCounters");

    appendTextChild(root, "totalRulesExecuted", static_cast<long long>(c.totalRulesExecuted.load()));
    appendTextChild(root, "totalRulesSuccessful", static_cast<long long>(c.totalRulesSuccessful.load()));
    appendTextChild(root, "totalRulesFailed", static_cast<long long>(c.totalRulesFailed.load()));
    appendTextChild(root, "totalRulesSkipped", static_cast<long long>(c.totalRulesSkipped.load()));
    appendTextChild(root, "totalRulesCached", static_cast<long long>(c.totalRulesCached.load()));
    appendTextChild(root, "totalRulesTimedOut", static_cast<long long>(c.totalRulesTimedOut.load()));
    appendTextChild(root, "totalRulesRateLimited", static_cast<long long>(c.totalRulesRateLimited.load()));
    appendTextChild(root, "totalCompileCount", static_cast<long long>(c.totalCompileCount.load()));
    appendTextChild(root, "totalCompileFailures", static_cast<long long>(c.totalCompileFailures.load()));
    appendTextChild(root, "totalExecutionTimeNs", static_cast<long long>(c.totalExecutionTimeNs.load()));
    appendTextChild(root, "averageExecutionTimeMs", counters.getAverageExecutionTimeMs());
    appendTextChild(root, "successRate", counters.getSuccessRate());
    appendTextChild(root, "cacheHitRate", counters.getCacheHitRate());

    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

// ============================================================================
// Workflow / Rule — delegates to XmlLoader
// ============================================================================

std::string XmlSerialization::serialize(const Workflow& workflow) {
    return XmlLoader::saveWorkflow(workflow);
}

Workflow XmlSerialization::deserializeWorkflow(const std::string& xml) {
    return XmlLoader::loadWorkflow(xml);
}

std::string XmlSerialization::serialize(const Rule& rule) {
    return XmlLoader::saveRule(rule);
}

std::shared_ptr<Rule> XmlSerialization::deserializeRule(const std::string& xml) {
    return XmlLoader::loadRule(xml);
}

} // namespace ext
} // namespace fastrules
