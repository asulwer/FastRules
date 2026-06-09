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
    virtual void save(const Rule& rule) = 0;
    virtual std::optional<Rule> findById(int id) = 0;
    virtual std::vector<Rule> findAll() = 0;
    virtual void remove(int id) = 0;
    virtual bool exists(int id) = 0;
    virtual size_t count() = 0;
};

/**
 * Abstract repository interface for Workflow persistence.
 */
class IWorkflowRepository {
public:
    virtual ~IWorkflowRepository() = default;
    virtual void save(const Workflow& workflow) = 0;
    virtual std::optional<Workflow> findById(int id) = 0;
    virtual std::vector<Workflow> findAll() = 0;
    virtual void remove(int id) = 0;
    virtual bool exists(int id) = 0;
    virtual size_t count() = 0;
};

/**
 * Abstract repository interface for RuleVersion history persistence.
 */
class IVersionRepository {
public:
    virtual ~IVersionRepository() = default;
    virtual void saveVersion(const RuleVersion& version, int ruleId) = 0;
    virtual std::vector<RuleVersion> findVersionsForRule(int ruleId) = 0;
    virtual std::optional<RuleVersion> findVersion(int ruleId, const std::string& versionId) = 0;
    virtual void removeAllVersionsForRule(int ruleId) = 0;
};

} // namespace ext
} // namespace fastrules
