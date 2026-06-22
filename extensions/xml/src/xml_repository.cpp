#include <fastrules/xml_repository.hpp>

#include <fastrules/rule.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <map>

namespace fastrules {
namespace ext {

// ============================================================================
// XmlRuleRepository
// ============================================================================

XmlRuleRepository::XmlRuleRepository(const std::filesystem::path& filepath)
    : filepath_(filepath) {
    load();
}

void XmlRuleRepository::save(const Rule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = doc_.child("rules");
    if (!root) {
        root = doc_.append_child("rules");
    }
    
    // Simple approach: remove all rules with same IDs, then add
    // Remove the main rule if it exists
    for (auto child = root.child("rule"); child; ) {
        auto next = child.next_sibling("rule");
        if (child.attribute("id").as_int() == rule.id) {
            root.remove_child(child);
        }
        child = next;
    }
    
    // Add the rule
    ruleToXml(rule, root);
    
    // Remove and add child rules
    for (const auto& childRule : rule.childRules) {
        if (childRule) {
            // Remove any existing child rule with the same ID
            for (auto child = root.child("rule"); child; ) {
                auto next = child.next_sibling("rule");
                if (child.attribute("id").as_int() == childRule->id) {
                    root.remove_child(child);
                }
                child = next;
            }
            
            ruleToXml(*childRule, root);
        }
    }
    
    dirty_ = true;
    if (dirty_) {
        write();
        dirty_ = false;
    }
}

std::optional<Rule> XmlRuleRepository::findById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = doc_.child("rules");
    if (!root) return std::nullopt;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        if (child.attribute("id").as_int() == id) {
            return xmlToRule(child);
        }
    }
    return std::nullopt;
}

std::vector<Rule> XmlRuleRepository::findAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Rule> rules;
    auto root = doc_.child("rules");
    if (!root) return rules;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        rules.push_back(xmlToRule(child));
    }
    return rules;
}

void XmlRuleRepository::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = doc_.child("rules");
    if (!root) return;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        if (child.attribute("id").as_int() == id) {
            root.remove_child(child);
            dirty_ = true;
            return;
        }
    }
}

bool XmlRuleRepository::exists(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = doc_.child("rules");
    if (!root) return false;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        if (child.attribute("id").as_int() == id) {
            return true;
        }
    }
    return false;
}

size_t XmlRuleRepository::count() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto root = doc_.child("rules");
    if (!root) return 0;
    
    size_t count = 0;
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        count++;
    }
    return count;
}

void XmlRuleRepository::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (dirty_) {
        write();
        dirty_ = false;
    }
}

void XmlRuleRepository::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::filesystem::exists(filepath_)) {
        std::ifstream file(filepath_);
        if (file.is_open()) {
            try {
                doc_.load(file);
            } catch (...) {
                doc_.reset();
            }
        }
    }
}

void XmlRuleRepository::write() {
    std::ofstream file(filepath_);
    if (file.is_open()) {
        doc_.save(file);
    }
}

std::string XmlRuleRepository::toString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return doc_.document_element().text().as_string();
}

void XmlRuleRepository::ruleToXml(const Rule& rule, pugi::xml_node& parent) const {
    auto node = parent.append_child("rule");
    node.append_attribute("id") = rule.id;
    if (!rule.name.empty()) {
        node.append_attribute("name") = rule.name.c_str();
    }
    node.append_attribute("isActive") = rule.isActive;
    node.append_attribute("priority") = rule.priority;
    if (rule.timeout) {
        node.append_attribute("timeout") = rule.timeout->count();
    }
    if (rule.dependsOnRuleName) {
        node.append_attribute("dependsOn") = rule.dependsOnRuleName->c_str();
    }
    
    if (!rule.expression.empty()) {
        node.append_child("expression").text() = rule.expression.c_str();
    }
    if (!rule.action.empty()) {
        node.append_child("action").text() = rule.action.c_str();
    }
}

Rule XmlRuleRepository::xmlToRule(const pugi::xml_node& node) const {
    Rule rule;
    
    try {
        rule.id = node.attribute("id").as_int(0);
        if (node.attribute("name")) {
            rule.name = node.attribute("name").as_string();
        }
        rule.isActive = node.attribute("isActive").as_bool(true);
        rule.priority = node.attribute("priority").as_int(0);
        if (node.attribute("timeout")) {
            rule.timeout = std::chrono::milliseconds(node.attribute("timeout").as_int());
        }
        if (node.attribute("dependsOn")) {
            rule.dependsOnRuleName = node.attribute("dependsOn").as_string();
        }
        
        auto exprNode = node.child("expression");
        if (exprNode) {
            rule.expression = exprNode.text().as_string();
        }
        
        auto actionNode = node.child("action");
        if (actionNode) {
            rule.action = actionNode.text().as_string();
        }
    } catch (...) {
        // Silently ignore any parse errors and return partially filled rule
    }
    
    return rule;
}

} // namespace ext
} // namespace fastrules