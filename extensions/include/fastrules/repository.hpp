#pragma once

#include <fastrules/rule.hpp>
#include <fastrules/workflow.hpp>
#include <fastrules/rule_versioning.hpp>
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace fastrules {
namespace ext {

/**
 * Abstract repository interface for Rule persistence.
 * Implemented by JSON, XML, and database extensions.
 */
class IRuleRepository {
public:
    virtual ~IRuleRepository() = default;
    
    /**
     * Save or update a rule.
     * If the rule already exists, it is updated.
     */
    virtual void save(const Rule& rule) = 0;
    
    /**
     * Find a rule by its unique ID.
     * Returns std::nullopt if not found.
     */
    virtual std::optional<Rule> findById(const std::string& id) = 0;
    
    /**
     * Retrieve all stored rules.
     */
    virtual std::vector<Rule> findAll() = 0;
    
    /**
     * Remove a rule by ID.
     * No-op if the rule does not exist.
     */
    virtual void remove(const std::string& id) = 0;
    
    /**
     * Check if a rule with the given ID exists.
     */
    virtual bool exists(const std::string& id) = 0;
    
    /**
     * Count total number of rules.
     */
    virtual size_t count() = 0;
};

/**
 * Abstract repository interface for Workflow persistence.
 */
class IWorkflowRepository {
public:
    virtual ~IWorkflowRepository() = default;
    
    virtual void save(const Workflow& workflow) = 0;
    virtual std::optional<Workflow> findById(const std::string& id) = 0;
    virtual std::vector<Workflow> findAll() = 0;
    virtual void remove(const std::string& id) = 0;
    virtual bool exists(const std::string& id) = 0;
    virtual size_t count() = 0;
};

/**
 * Abstract repository interface for RuleVersion history persistence.
 */
class IVersionRepository {
public:
    virtual ~IVersionRepository() = default;
    
    virtual void saveVersion(const RuleVersion& version, const std::string& ruleId) = 0;
    virtual std::vector<RuleVersion> findVersionsForRule(const std::string& ruleId) = 0;
    virtual std::optional<RuleVersion> findVersion(const std::string& ruleId, const std::string& versionId) = 0;
    virtual void removeAllVersionsForRule(const std::string& ruleId) = 0;
};

} // namespace ext
} // namespace fastrules
