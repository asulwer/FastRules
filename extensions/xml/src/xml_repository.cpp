#include <fastrules/xml_repository.hpp>

#include <fastrules/rule.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <atomic>
#include <mutex>
#include <map>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>

namespace fastrules {
namespace ext {

namespace {

/// Build a temp path unique to this writer.
///
/// A fixed "<target>.tmp" is not enough: several repository instances (threads,
/// or separate processes) may target the same file, and they would then
/// truncate and rename each other's scratch file. A unique name keeps each
/// write self-contained; the final rename decides which one wins.
std::filesystem::path makeTempPath(const std::filesystem::path& target) {
    static std::atomic<unsigned long long> counter{0};
    std::ostringstream suffix;
    suffix << ".tmp." << std::this_thread::get_id() << '.'
           << counter.fetch_add(1, std::memory_order_relaxed);
    std::filesystem::path tmp = target;
    tmp += suffix.str();
    return tmp;
}


/// Replace @p target with @p tmp, retrying briefly on transient failures.
///
/// On Windows a rename over a destination that another thread or process
/// currently has open (e.g. a concurrent load()) fails with a sharing
/// violation. That is contention, not corruption, so retry for a short while
/// before reporting failure.
void atomicReplace(const std::filesystem::path& tmp, const std::filesystem::path& target) {
    std::error_code ec;
    for (int attempt = 0; attempt < 40; ++attempt) {
        std::filesystem::rename(tmp, target, ec);
        if (!ec) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::filesystem::remove(tmp, ec);
    throw std::runtime_error("Failed to replace " + target.string() + " with " + tmp.string());
}

}  // namespace

// ============================================================================
// XmlRuleRepository
// ============================================================================

XmlRuleRepository::XmlRuleRepository(const std::filesystem::path& filepath)
    : filepath_(filepath) {
    load();
}

XmlRuleRepository::~XmlRuleRepository() {
    // Honour the "written on destruction" contract shared with
    // JsonRuleRepository. Never throw out of a destructor.
    try {
        flush();
    } catch (...) {
    }
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
    
    // Writes are batched; persisted on flush() or destruction (same contract
    // as JsonRuleRepository).
    dirty_ = true;
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
    // Write to a temp file then rename, so an interrupted write cannot leave a
    // truncated rule store. Throws on failure so flush() does not clear
    // dirty_ for a write that never landed.
    std::filesystem::path tmp = makeTempPath(filepath_);

    {
        std::ofstream file(tmp, std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("XmlRuleRepository: cannot open " + tmp.string() + " for writing");
        }
        doc_.save(file);
        file.flush();
        if (!file) {
            throw std::runtime_error("XmlRuleRepository: failed writing " + tmp.string());
        }
    }

    atomicReplace(tmp, filepath_);
}

std::string XmlRuleRepository::toString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Serialise the whole document. The previous implementation returned
    // document_element().text(), i.e. the character data directly inside
    // <rules> - which is empty for this schema, so it always returned "".
    std::ostringstream oss;
    doc_.save(oss);
    return oss.str();
}

void XmlRuleRepository::ruleToXml(const Rule& rule, pugi::xml_node& parent) const {
    auto node = parent.append_child("rule");
    node.append_attribute("id") = rule.id;
    if (!rule.name.empty()) {
        node.append_attribute("name") = rule.name.c_str();
    }
    if (!rule.description.empty()) {
        node.append_attribute("description") = rule.description.c_str();
    }
    node.append_attribute("isActive") = rule.isActive;
    node.append_attribute("priority") = rule.priority;
    if (rule.timeout) {
        node.append_attribute("timeout") = static_cast<long long>(rule.timeout->count());
    }
    if (rule.cacheDuration) {
        node.append_attribute("cacheDuration") = static_cast<long long>(rule.cacheDuration->count());
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
        if (node.attribute("description")) {
            rule.description = node.attribute("description").as_string();
        }
        rule.isActive = node.attribute("isActive").as_bool(true);
        rule.priority = node.attribute("priority").as_int(0);
        if (node.attribute("timeout")) {
            rule.timeout = std::chrono::milliseconds(node.attribute("timeout").as_llong());
        }
        if (node.attribute("cacheDuration")) {
            rule.cacheDuration = std::chrono::milliseconds(node.attribute("cacheDuration").as_llong());
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