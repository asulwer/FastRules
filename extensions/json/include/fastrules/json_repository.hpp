#pragma once

#include <fastrules/repository.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <mutex>

namespace fastrules {
namespace ext {

/**
 * JSON file-based implementation of IRuleRepository.
 * 
 * Stores rules in a single JSON file as an array:
 * [
 *   {
 *     "id": "rule-1",
 *     "expression": "age >= 18",
 *     "action": "eligible = true",
 *     "isActive": true,
 *     "priority": 0,
 *     "timeout": 100,
 *     "dependencyChain": [],
 *     "childRuleIds": [],
 *     "version": "1.0.0"
 *   }
 * ]
 */
class JsonRuleRepository : public IRuleRepository {
public:
    explicit JsonRuleRepository(const std::filesystem::path& filepath);
    ~JsonRuleRepository() override = default;
    
    void save(const Rule& rule) override;
    std::optional<Rule> findById(int id) override;
    std::vector<Rule> findAll() override;
    void remove(int id) override;
    bool exists(int id) override;
    size_t count() override;
    
    /**
     * Force immediate write to disk.
     * By default, saves are batched and written on destruction.
     */
    void flush();
    
    /**
     * Get the file path being used.
     */
    std::filesystem::path filepath() const { return filepath_; }

private:
    std::filesystem::path filepath_;
    nlohmann::json data_;
    bool dirty_ = false;
    mutable std::mutex mutex_;
    
    void load();
    void write();
    nlohmann::json ruleToJson(const Rule& rule) const;
    Rule jsonToRule(const nlohmann::json& j) const;
};

/**
 * JSON file-based implementation of IWorkflowRepository.
 */
class JsonWorkflowRepository : public IWorkflowRepository {
public:
    explicit JsonWorkflowRepository(const std::filesystem::path& filepath);
    ~JsonWorkflowRepository() override = default;
    
    void save(const Workflow& workflow) override;
    std::optional<Workflow> findById(int id) override;
    std::vector<Workflow> findAll() override;
    void remove(int id) override;
    bool exists(int id) override;
    size_t count() override;
    
    void flush();
    std::filesystem::path filepath() const { return filepath_; }

private:
    std::filesystem::path filepath_;
    nlohmann::json data_;
    bool dirty_ = false;
    
    void load();
    void write();
};

} // namespace ext
} // namespace fastrules
