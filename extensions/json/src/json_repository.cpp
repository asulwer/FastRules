#include <fastrules/json_repository.hpp>

#include <fastrules/rule.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
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
// JsonRuleRepository
// ============================================================================

JsonRuleRepository::JsonRuleRepository(const std::filesystem::path& filepath)
    : filepath_(filepath) {
    load();
}

JsonRuleRepository::~JsonRuleRepository() {
    // Honor the documented "written on destruction" contract. Never throw.
    try {
        flush();
    } catch (...) {
    }
}

void JsonRuleRepository::upsert(nlohmann::json&& item) {
    // Replace the existing entry with the same id, otherwise append. This keeps
    // save() O(n) instead of rebuilding a dedup map (and deep-copying every
    // stored rule) on each call.
    if (item.contains("id")) {
        const auto& id = item["id"];
        for (auto& existing : data_) {
            if (existing.contains("id") && existing["id"] == id) {
                existing = std::move(item);
                return;
            }
        }
    }
    data_.push_back(std::move(item));
}

void JsonRuleRepository::save(const Rule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);

    upsert(ruleToJson(rule));

    // Save child rules
    for (const auto& childRule : rule.childRules) {
        if (childRule) {
            upsert(ruleToJson(*childRule));
        }
    }

    // Writes are batched; persisted on flush() or destruction.
    dirty_ = true;
}

std::optional<Rule> JsonRuleRepository::findById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& item : data_) {
        if (item.contains("id") && item["id"] == id) {
            return jsonToRule(item);
        }
    }
    return std::nullopt;
}

std::vector<Rule> JsonRuleRepository::findAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Rule> rules;
    rules.reserve(data_.size());
    for (const auto& item : data_) {
        rules.push_back(jsonToRule(item));
    }
    return rules;
}

void JsonRuleRepository::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& item : data_) {
        if (item.contains("id") && item["id"] == id) {
            return true;
        }
    }
    return false;
}

size_t JsonRuleRepository::count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

void JsonRuleRepository::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (dirty_) {
        // Only clear the dirty flag once the data really reached disk.
        // Clearing it unconditionally silently discarded pending changes when
        // the file could not be written (permissions, full disk, bad path).
        write();
        dirty_ = false;
    }
}

void JsonRuleRepository::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = nlohmann::json::array();
    if (!std::filesystem::exists(filepath_)) {
        return;
    }
    std::ifstream file(filepath_);
    if (!file.is_open()) {
        return;
    }
    try {
        nlohmann::json parsed;
        file >> parsed;
        // Guard the shape as well as the syntax: a file holding valid JSON
        // that is not an array (e.g. an object) would make every later
        // push_back throw nlohmann::type_error.
        if (parsed.is_array()) {
            data_ = std::move(parsed);
        }
    } catch (const nlohmann::json::exception&) {
        // Leave data_ as the empty array initialised above.
    }
}

void JsonRuleRepository::write() {
    // Write to a temporary file and rename over the target, so an interrupted
    // or failed write cannot leave a truncated/corrupt rule store behind.
    // Throws on failure so flush() does not clear dirty_ for a write that
    // never landed.
    std::filesystem::path tmp = makeTempPath(filepath_);

    {
        std::ofstream file(tmp, std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("JsonRuleRepository: cannot open " + tmp.string() + " for writing");
        }
        file << data_.dump(2);
        file.flush();
        if (!file) {
            throw std::runtime_error("JsonRuleRepository: failed writing " + tmp.string());
        }
    }

    atomicReplace(tmp, filepath_);
}

nlohmann::json JsonRuleRepository::ruleToJson(const Rule& rule) const {
    nlohmann::json j;
    j["id"] = rule.id;
    if (!rule.name.empty()) {
        j["name"] = rule.name;
    }
    if (!rule.description.empty()) {
        j["description"] = rule.description;
    }
    j["expression"] = rule.expression;
    j["action"] = rule.action;
    j["isActive"] = rule.isActive;
    j["priority"] = rule.priority;
    if (rule.timeout) {
        j["timeout"] = rule.timeout->count();
    }
    if (rule.cacheDuration) {
        j["cacheDuration"] = rule.cacheDuration->count();
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
            if (j.contains("name") && j["name"].is_string()) {
                rule.name = j["name"].get<std::string>();
            }
            if (j.contains("description") && j["description"].is_string()) {
                rule.description = j["description"].get<std::string>();
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
            if (j.contains("cacheDuration") && j["cacheDuration"].is_number_integer()) {
                rule.cacheDuration = std::chrono::milliseconds(j["cacheDuration"].get<long long>());
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