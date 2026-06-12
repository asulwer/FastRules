#include <fastrules\json_repository.hpp>

namespace fastrules {
namespace ext {

// ============================================================================
// JsonRuleRepository
// ============================================================================

JsonRuleRepository::JsonRuleRepository(const std::filesystem::path& filepath)
    : filepath_(filepath) {
    load();
}

void JsonRuleRepository::save(const Rule& rule) {
    bool found = false;
    for (auto& item : data_) {
        if (item.contains("id") && item["id"] == rule.id) {
            item = ruleToJson(rule);
            found = true;
            break;
        }
    }
    if (!found) {
        data_.push_back(ruleToJson(rule));
    }
    dirty_ = true;
}

std::optional<Rule> JsonRuleRepository::findById(int id) {
    for (const auto& item : data_) {
        if (item.contains("id") && item["id"] == id) {
            return jsonToRule(item);
        }
    }
    return std::nullopt;
}

std::vector<Rule> JsonRuleRepository::findAll() {
    std::vector<Rule> rules;
    rules.reserve(data_.size());
    for (const auto& item : data_) {
        rules.push_back(jsonToRule(item));
    }
    return rules;
}

void JsonRuleRepository::remove(int id) {
    auto it = std::remove_if(data_.begin(), data_.end(),
        [id](const nlohmann::json& item) {
            return item.contains("id") && item["id"] == id;
        });
    if (it != data_.end()) {
        data_.erase(it, data_.end());
        dirty_ = true;
    }
}

bool JsonRuleRepository::exists(int id) {
    for (const auto& item : data_) {
        if (item.contains("id") && item["id"] == id) {
            return true;
        }
    }
    return false;
}

size_t JsonRuleRepository::count() {
    return data_.size();
}

void JsonRuleRepository::flush() {
    if (dirty_) {
        write();
        dirty_ = false;
    }
}

void JsonRuleRepository::load() {
    if (std::filesystem::exists(filepath_)) {
        std::ifstream file(filepath_);
        if (file.is_open()) {
            try {
                file >> data_;
            } catch (const nlohmann::json::exception&) {
                data_ = nlohmann::json::array();
            }
        } else {
            data_ = nlohmann::json::array();
        }
    } else {
        data_ = nlohmann::json::array();
    }
}

void JsonRuleRepository::write() {
    std::ofstream file(filepath_);
    if (file.is_open()) {
        file << data_.dump(2);
    }
}

nlohmann::json JsonRuleRepository::ruleToJson(const Rule& rule) const {
    nlohmann::json j;
    j["id"] = rule.id;
    j["expression"] = rule.expression;
    j["action"] = rule.action;
    j["isActive"] = rule.isActive;
    j["priority"] = rule.priority;
    if (rule.timeout) {
        j["timeout"] = rule.timeout->count();
    }
    if (rule.dependsOnRuleName) {
        j["dependsOn"] = *rule.dependsOnRuleName;
    }
    return j;
}

Rule JsonRuleRepository::jsonToRule(const nlohmann::json& j) const {
    Rule rule;
    
    try {
        if (j.is_object()) {
            if (j.contains("id") && j["id"].is_number_integer()) {
                rule.id = j["id"].get<int>();
            }
            if (j.contains("expression") && j["expression"].is_string()) {
                rule.expression = j["expression"].get<std::string>();
            }
            if (j.contains("action") && j["action"].is_string()) {
                rule.action = j["action"].get<std::string>();
            }
            if (j.contains("isActive") && j["isActive"].is_boolean()) {
                rule.isActive = j["isActive"].get<bool>();
            }
            if (j.contains("priority") && j["priority"].is_number_integer()) {
                rule.priority = j["priority"].get<int>();
            }
            if (j.contains("timeout") && j["timeout"].is_number_integer()) {
                rule.timeout = std::chrono::milliseconds(j["timeout"].get<int>());
            }
            if (j.contains("dependsOn") && j["dependsOn"].is_string()) {
                rule.dependsOnRuleName = j["dependsOn"].get<std::string>();
            }
        }
    } catch (...) {
        // Silently ignore any parse errors and return partially filled rule
    }
    
    return rule;
}

} // namespace ext
} // namespace fastrules
