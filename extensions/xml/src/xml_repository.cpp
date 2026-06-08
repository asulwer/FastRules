#include <fastrules\xml_repository.hpp>

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
    auto root = doc_.child("rules");
    if (!root) {
        root = doc_.append_child("rules");
    }
    
    // Remove existing rule with same ID
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        if (std::string(child.attribute("id").value()) == rule.id) {
            root.remove_child(child);
            break;
        }
    }
    
    ruleToXml(rule, root);
    dirty_ = true;
}

std::optional<Rule> XmlRuleRepository::findById(const std::string& id) {
    auto root = doc_.child("rules");
    if (!root) return std::nullopt;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        if (std::string(child.attribute("id").value()) == id) {
            return xmlToRule(child);
        }
    }
    return std::nullopt;
}

std::vector<Rule> XmlRuleRepository::findAll() {
    std::vector<Rule> rules;
    auto root = doc_.child("rules");
    if (!root) return rules;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        rules.push_back(xmlToRule(child));
    }
    return rules;
}

void XmlRuleRepository::remove(const std::string& id) {
    auto root = doc_.child("rules");
    if (!root) return;
    
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        if (std::string(child.attribute("id").value()) == id) {
            root.remove_child(child);
            dirty_ = true;
            return;
        }
    }
}

bool XmlRuleRepository::exists(const std::string& id) {
    return findById(id).has_value();
}

size_t XmlRuleRepository::count() {
    auto root = doc_.child("rules");
    if (!root) return 0;
    
    size_t n = 0;
    for (auto child = root.child("rule"); child; child = child.next_sibling("rule")) {
        ++n;
    }
    return n;
}

void XmlRuleRepository::flush() {
    if (dirty_) {
        write();
        dirty_ = false;
    }
}

std::string XmlRuleRepository::toString() const {
    std::ostringstream oss;
    doc_.save(oss, "  ");
    return oss.str();
}

void XmlRuleRepository::load() {
    if (std::filesystem::exists(filepath_)) {
        auto result = doc_.load_file(filepath_.c_str());
        if (!result) {
            doc_.reset();
            doc_.append_child("rules");
        }
    } else {
        doc_.append_child("rules");
    }
}

void XmlRuleRepository::write() {
    doc_.save_file(filepath_.c_str(), "  ");
}

void XmlRuleRepository::ruleToXml(const Rule& rule, pugi::xml_node& parent) const {
    auto node = parent.append_child("rule");
    node.append_attribute("id") = rule.id.c_str();
    node.append_attribute("isActive") = rule.isActive;
    node.append_attribute("priority") = rule.priority;
    if (rule.timeout) {
        node.append_attribute("timeout") = static_cast<int>(rule.timeout->count());
    }
    
    auto expr = node.append_child("expression");
    expr.append_child(pugi::node_pcdata).set_value(rule.expression.c_str());
    
    auto act = node.append_child("action");
    act.append_child(pugi::node_pcdata).set_value(rule.action.c_str());
    
    auto params = node.append_child("parameters");
    for (const auto& p : rule.parameterNames) {
        auto param = params.append_child("param");
        param.append_child(pugi::node_pcdata).set_value(p.c_str());
    }
    
    if (rule.dependsOnRuleId) {
        auto deps = node.append_child("dependencies");
        auto dep = deps.append_child("dep");
        dep.append_child(pugi::node_pcdata).set_value(rule.dependsOnRuleId->c_str());
    }
}

Rule XmlRuleRepository::xmlToRule(const pugi::xml_node& node) const {
    Rule rule;
    rule.id = node.attribute("id").value();
    rule.expression = node.child("expression").child_value();
    rule.action = node.child("action").child_value();
    
    rule.isActive = node.attribute("isActive").as_bool(true);
    rule.priority = node.attribute("priority").as_int(0);
    if (node.attribute("timeout")) {
        rule.timeout = std::chrono::milliseconds(node.attribute("timeout").as_int(100));
    }
    
    auto params = node.child("parameters");
    for (auto p = params.child("param"); p; p = p.next_sibling("param")) {
        rule.parameterNames.push_back(p.child_value());
    }
    
    auto deps = node.child("dependencies");
    for (auto d = deps.child("dep"); d; d = d.next_sibling("dep")) {
        rule.dependsOnRuleId = d.child_value();
    }
    
    return rule;
}

} // namespace ext
} // namespace fastrules
