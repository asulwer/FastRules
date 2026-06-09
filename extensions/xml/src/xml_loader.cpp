#include <fastrules/xml_loader.hpp>
#include <pugixml.hpp>

#include <stdexcept>
#include <fstream>
#include <sstream>
#include <optional>
#include <chrono>

namespace fastrules {

static std::shared_ptr<Rule> parseRule(const pugi::xml_node& ruleNode) {
    auto rule = std::make_shared<Rule>();

    rule->id = ruleNode.attribute("id").as_int(0);
    rule->isActive = ruleNode.attribute("isActive").as_bool(true);
    rule->priority = ruleNode.attribute("priority").as_int(0);

    auto exprNode = ruleNode.child("expression");
    if (exprNode) {
        rule->expression = exprNode.text().as_string("");
    }

    auto actionNode = ruleNode.child("action");
    if (actionNode) {
        rule->action = actionNode.text().as_string("");
    }

    auto descNode = ruleNode.child("description");
    if (descNode) {
        rule->description = descNode.text().as_string("");
    }

    auto timeoutAttr = ruleNode.attribute("timeout");
    if (timeoutAttr) {
        rule->timeout = std::chrono::milliseconds(timeoutAttr.as_int(0));
    }

    auto cacheAttr = ruleNode.attribute("cacheDuration");
    if (cacheAttr) {
        rule->cacheDuration = std::chrono::milliseconds(cacheAttr.as_int(0));
    }

    auto dependsNode = ruleNode.child("dependsOnRuleId");
    if (dependsNode) {
        rule->dependsOnRuleId = dependsNode.text().as_int(0);
    }

    auto childrenNode = ruleNode.child("childRules");
    if (childrenNode) {
        for (auto child : childrenNode.children("rule")) {
            rule->childRules.push_back(parseRule(child));
        }
    }

    return rule;
}

static void serializeRule(pugi::xml_node& parent, const Rule& rule) {
    auto node = parent.append_child("rule");
    node.append_attribute("id").set_value(rule.id);
    node.append_attribute("isActive").set_value(rule.isActive);
    node.append_attribute("priority").set_value(rule.priority);

    if (rule.timeout.has_value()) {
        node.append_attribute("timeout").set_value(static_cast<int>(rule.timeout->count()));
    }
    if (rule.cacheDuration.has_value()) {
        node.append_attribute("cacheDuration").set_value(static_cast<int>(rule.cacheDuration->count()));
    }

    if (!rule.description.empty()) {
        auto desc = node.append_child("description");
        desc.text().set(rule.description.c_str());
    }
    if (!rule.expression.empty()) {
        auto expr = node.append_child("expression");
        expr.text().set(rule.expression.c_str());
    }
    if (!rule.action.empty()) {
        auto act = node.append_child("action");
        act.text().set(rule.action.c_str());
    }
    if (rule.dependsOnRuleId.has_value()) {
        auto dep = node.append_child("dependsOnRuleId");
        dep.text().set(rule.dependsOnRuleId.value());
    }
    if (!rule.childRules.empty()) {
        auto children = node.append_child("childRules");
        for (const auto& child : rule.childRules) {
            serializeRule(children, *child);
        }
    }
}

Workflow XmlLoader::loadWorkflow(const std::string& xmlString) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlString.c_str());
    if (!result) {
        throw std::runtime_error(std::string("XML parse error: ") + result.description());
    }

    auto root = doc.child("workflow");
    if (!root) {
        throw std::runtime_error("XML missing <workflow> root element");
    }

    Workflow workflow;
    workflow.id = root.attribute("id").as_int(0);
    workflow.description = root.attribute("description").as_string("");
    workflow.isActive = root.attribute("isActive").as_bool(true);

    auto rulesNode = root.child("rules");
    if (rulesNode) {
        for (auto ruleNode : rulesNode.children("rule")) {
            workflow.rules.push_back(parseRule(ruleNode));
        }
    }

    return workflow;
}

Workflow XmlLoader::loadWorkflowFromFile(const std::string& filePath) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.c_str());
    if (!result) {
        throw std::runtime_error(std::string("XML file parse error: ") + result.description());
    }

    auto root = doc.child("workflow");
    if (!root) {
        throw std::runtime_error("XML missing <workflow> root element");
    }

    Workflow workflow;
    workflow.id = root.attribute("id").as_int(0);
    workflow.description = root.attribute("description").as_string("");
    workflow.isActive = root.attribute("isActive").as_bool(true);

    auto rulesNode = root.child("rules");
    if (rulesNode) {
        for (auto ruleNode : rulesNode.children("rule")) {
            workflow.rules.push_back(parseRule(ruleNode));
        }
    }

    return workflow;
}

std::shared_ptr<Rule> XmlLoader::loadRule(const std::string& xmlString) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlString.c_str());
    if (!result) {
        throw std::runtime_error(std::string("XML parse error: ") + result.description());
    }

    auto root = doc.child("rule");
    if (!root) {
        throw std::runtime_error("XML missing <rule> root element");
    }

    return parseRule(root);
}

std::string XmlLoader::saveWorkflow(const Workflow& workflow) {
    pugi::xml_document doc;
    auto root = doc.append_child("workflow");
    root.append_attribute("id").set_value(workflow.id.c_str());
    root.append_attribute("description").set_value(workflow.description.c_str());
    root.append_attribute("isActive").set_value(workflow.isActive);

    auto rulesNode = root.append_child("rules");
    for (const auto& rule : workflow.rules) {
        serializeRule(rulesNode, *rule);
    }

    std::ostringstream oss;
    doc.save(oss);
    return oss.str();
}

std::string XmlLoader::saveWorkflowPretty(const Workflow& workflow) {
    pugi::xml_document doc;
    auto root = doc.append_child("workflow");
    root.append_attribute("id").set_value(workflow.id.c_str());
    root.append_attribute("description").set_value(workflow.description.c_str());
    root.append_attribute("isActive").set_value(workflow.isActive);

    auto rulesNode = root.append_child("rules");
    for (const auto& rule : workflow.rules) {
        serializeRule(rulesNode, *rule);
    }

    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

std::string XmlLoader::saveRule(const Rule& rule) {
    pugi::xml_document doc;
    serializeRule(doc, rule);
    std::ostringstream oss;
    doc.save(oss);
    return oss.str();
}

std::string XmlLoader::saveRulePretty(const Rule& rule) {
    pugi::xml_document doc;
    serializeRule(doc, rule);
    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

} // namespace fastrules
