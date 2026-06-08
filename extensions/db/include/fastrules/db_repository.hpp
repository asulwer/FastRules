#pragma once

#include <fastrules/repository.hpp>
#include <fastrules/db_bool.hpp>
#include <soci/soci.h>
#include <memory>
#include <string>

namespace fastrules {
namespace ext {

/**
 * Database-backed implementation of IRuleRepository using SOCI.
 *
 * Supports PostgreSQL, MySQL, SQLite, Oracle, SQL Server via SOCI backends.
 *
 * Connection string format (backend-specific):
 *   PostgreSQL: "dbname=rules host=localhost user=fastrules password=***"
 *   MySQL:      "db=rules user=fastrules password=***"
 *   SQLite:     "rules.db"
 */
class DbRuleRepository : public IRuleRepository {
public:
    /**
     * Construct with SOCI session.
     * The session must already be connected to the database.
     */
    explicit DbRuleRepository(std::shared_ptr<soci::session> session);
    ~DbRuleRepository() override = default;
    
    void save(const Rule& rule) override;
    std::optional<Rule> findById(const std::string& id) override;
    std::vector<Rule> findAll() override;
    void remove(const std::string& id) override;
    bool exists(const std::string& id) override;
    size_t count() override;
    
    /**
     * Create the database schema if it doesn't exist.
     * Safe to call multiple times (idempotent).
     */
    void createSchema();
    
    /**
     * Delete all rules. Use with caution.
     */
    void clear();
    
    /**
     * Get the underlying SOCI session.
     */
    soci::session& session() { return *session_; }

private:
    std::shared_ptr<soci::session> session_;
    
    Rule rowToRule(soci::row& row);
    void insertRule(const Rule& rule);
    void updateRule(const Rule& rule);
    
    friend class DbWorkflowRepository;
};

/**
 * Database-backed implementation of IWorkflowRepository using SOCI.
 */
class DbWorkflowRepository : public IWorkflowRepository {
public:
    explicit DbWorkflowRepository(std::shared_ptr<soci::session> session);
    ~DbWorkflowRepository() override = default;
    
    void save(const Workflow& workflow) override;
    std::optional<Workflow> findById(const std::string& id) override;
    std::vector<Workflow> findAll() override;
    void remove(const std::string& id) override;
    bool exists(const std::string& id) override;
    size_t count() override;
    
    void createSchema();
    void clear();
    soci::session& session() { return *session_; }

private:
    std::shared_ptr<soci::session> session_;
};

/**
 * Database-backed implementation of IVersionRepository using SOCI.
 */
class DbVersionRepository : public IVersionRepository {
public:
    explicit DbVersionRepository(std::shared_ptr<soci::session> session);
    ~DbVersionRepository() override = default;
    
    void saveVersion(const RuleVersion& version, const std::string& ruleId) override;
    std::vector<RuleVersion> findVersionsForRule(const std::string& ruleId) override;
    std::optional<RuleVersion> findVersion(const std::string& ruleId, const std::string& versionId) override;
    void removeAllVersionsForRule(const std::string& ruleId) override;
    
    void createSchema();
    soci::session& session() { return *session_; }

private:
    std::shared_ptr<soci::session> session_;
};

/**
 * Convenience factory for creating database connections.
 */
class DbConnectionFactory {
public:
    /**
     * Create a SOCI session for the given backend.
     * Backend: "postgresql", "mysql", "sqlite3", "odbc"
     */
    static std::shared_ptr<soci::session> create(
        const std::string& backend,
        const std::string& connectionString);
    
    /**
     * Create all repositories for a given session.
     * Also creates schema if needed.
     */
    static void initializeSchema(const std::shared_ptr<soci::session>& session);
};

} // namespace ext
} // namespace fastrules
