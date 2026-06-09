#include <fastrules\db_repository.hpp>
#include <fastrules\db_bool.hpp>
#include <soci\sqlite3\soci-sqlite3.h>

// Optional backends — only include if available
#if __has_include(<soci\postgresql.h>)
#include <soci\postgresql.h>
#endif
#if __has_include(<soci\mysql.h>)
#include <soci\mysql.h>
#endif

namespace fastrules {
namespace ext {

// ============================================================================
// DbRuleRepository
// ============================================================================

DbRuleRepository::DbRuleRepository(std::shared_ptr<soci::session> session)
    : session_(std::move(session)) {
    createSchema();
}

void DbRuleRepository::createSchema() {
    *session_ << 
        "CREATE TABLE IF NOT EXISTS rules ("
        "  id VARCHAR(255) PRIMARY KEY,"
        "  expression TEXT NOT NULL,"
        "  action TEXT,"
        "  description TEXT,"
        "  is_active BOOLEAN DEFAULT TRUE,"
        "  priority INT DEFAULT 0,"
        "  timeout_ms INT,"
        "  cache_duration_ms INT,"
        "  depends_on_rule_id VARCHAR(255),"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")";
    
    *session_ <<
        "CREATE TABLE IF NOT EXISTS rule_parameters ("
        "  rule_id VARCHAR(255) REFERENCES rules(id) ON DELETE CASCADE,"
        "  param_name VARCHAR(255),"
        "  param_order INT,"
        "  PRIMARY KEY (rule_id, param_name)"
        ")";
}

void DbRuleRepository::save(const Rule& rule) {
    soci::transaction tr(*session_);
    
    if (exists(rule.id)) {
        updateRule(rule);
    } else {
        insertRule(rule);
    }
    
    tr.commit();
}

void DbRuleRepository::insertRule(const Rule& rule) {
    int timeoutMs = rule.timeout.has_value() ? static_cast<int>(rule.timeout->count()) : -1;
    int cacheMs   = rule.cacheDuration.has_value() ? static_cast<int>(rule.cacheDuration->count()) : -1;
    int dependsOn = rule.dependsOnRuleId.has_value() ? rule.dependsOnRuleId.value() : -1;
    DbBool isActive = rule.isActive;

    *session_ << 
        "INSERT INTO rules (id, expression, action, description, is_active, priority, timeout_ms, cache_duration_ms, depends_on_rule_id) "
        "VALUES (:id, :expr, :act, :desc, :active, :prio, :timeout, :cache, :dep)",
        soci::use(rule.id), soci::use(rule.expression), soci::use(rule.action), soci::use(rule.description),
        soci::use(isActive), soci::use(rule.priority), soci::use(timeoutMs), soci::use(cacheMs), soci::use(dependsOn);
}

void DbRuleRepository::updateRule(const Rule& rule) {
    int timeoutMs = rule.timeout.has_value() ? static_cast<int>(rule.timeout->count()) : -1;
    int cacheMs   = rule.cacheDuration.has_value() ? static_cast<int>(rule.cacheDuration->count()) : -1;
    int dependsOn = rule.dependsOnRuleId.has_value() ? rule.dependsOnRuleId.value() : -1;
    DbBool isActive = rule.isActive;

    *session_ <<
        "UPDATE rules SET "
        "  expression = :expr, action = :act, description = :desc, is_active = :active, "
        "  priority = :prio, timeout_ms = :timeout, cache_duration_ms = :cache, depends_on_rule_id = :dep, "
        "  updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id",
        soci::use(rule.expression), soci::use(rule.action), soci::use(rule.description),
        soci::use(isActive), soci::use(rule.priority),
        soci::use(timeoutMs), soci::use(cacheMs), soci::use(dependsOn), soci::use(rule.id);
}

std::optional<Rule> DbRuleRepository::findById(int id) {
    soci::rowset<soci::row> rs = ((*session_).prepare <<
        "SELECT id, expression, action, description, is_active, priority, timeout_ms, cache_duration_ms, depends_on_rule_id "
        "FROM rules WHERE id = :id", soci::use(id));
    
    for (auto& row : rs) {
        return rowToRule(row);
    }
    return std::nullopt;
}

std::vector<Rule> DbRuleRepository::findAll() {
    std::vector<Rule> rules;
    soci::rowset<soci::row> rs = ((*session_).prepare <<
        "SELECT id, expression, action, description, is_active, priority, timeout_ms, cache_duration_ms, depends_on_rule_id FROM rules");
    
    for (auto& row : rs) {
        rules.push_back(rowToRule(row));
    }
    return rules;
}

void DbRuleRepository::remove(int id) {
    *session_ << "DELETE FROM rules WHERE id = :id", soci::use(id);
}

bool DbRuleRepository::exists(int id) {
    int count = 0;
    *session_ << "SELECT COUNT(*) FROM rules WHERE id = :id", soci::into(count), soci::use(id);
    return count > 0;
}

size_t DbRuleRepository::count() {
    size_t count = 0;
    *session_ << "SELECT COUNT(*) FROM rules", soci::into(count);
    return count;
}

void DbRuleRepository::clear() {
    *session_ << "DELETE FROM rule_parameters";
    *session_ << "DELETE FROM rules";
}

Rule DbRuleRepository::rowToRule(soci::row& row) {
    Rule rule;
    rule.id = row.get<int>(0);
    rule.expression = row.get<std::string>(1);
    rule.action = row.get<std::string>(2);
    rule.description = row.get<std::string>(3);
    DbBool isActive;
    isActive = row.get<int>(4) != 0;
    rule.isActive = isActive;
    rule.priority = row.get<int>(5);
    
    int timeoutMs = row.get<int>(6);
    if (timeoutMs >= 0) rule.timeout = std::chrono::milliseconds(timeoutMs);
    
    int cacheMs = row.get<int>(7);
    if (cacheMs >= 0) rule.cacheDuration = std::chrono::milliseconds(cacheMs);
    
    int dependsOn = row.get<int>(8);
    if (dependsOn >= 0) rule.dependsOnRuleId = dependsOn;
    
    return rule;
}

// ============================================================================
// DbWorkflowRepository
// ============================================================================

DbWorkflowRepository::DbWorkflowRepository(std::shared_ptr<soci::session> session)
    : session_(std::move(session)) {
    createSchema();
}

void DbWorkflowRepository::createSchema() {
    *session_ <<
        "CREATE TABLE IF NOT EXISTS workflows ("
        "  id INTEGER PRIMARY KEY,"
        "  description TEXT,"
        "  is_active BOOLEAN DEFAULT TRUE,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")";

    *session_ <<
        "CREATE TABLE IF NOT EXISTS workflow_rules ("
        "  workflow_id INTEGER REFERENCES workflows(id) ON DELETE CASCADE,"
        "  rule_id INTEGER REFERENCES rules(id) ON DELETE CASCADE,"
        "  rule_order INT,"
        "  PRIMARY KEY (workflow_id, rule_id)"
        ")";
}

void DbWorkflowRepository::save(const Workflow& workflow) {
    soci::transaction tr(*session_);

    if (exists(workflow.id)) {
        DbBool isActive = workflow.isActive;
        *session_ <<
            "UPDATE workflows SET "
            "  description = :desc, is_active = :active, updated_at = CURRENT_TIMESTAMP "
            "WHERE id = :id",
            soci::use(workflow.description), soci::use(isActive), soci::use(workflow.id);

        *session_ << "DELETE FROM workflow_rules WHERE workflow_id = :id", soci::use(workflow.id);
    } else {
        DbBool isActive = workflow.isActive;
        *session_ <<
            "INSERT INTO workflows (id, description, is_active) "
            "VALUES (:id, :desc, :active)",
            soci::use(workflow.id), soci::use(workflow.description), soci::use(isActive);
    }

    for (size_t i = 0; i < workflow.rules.size(); ++i) {
        const auto& rule = workflow.rules[i];
        *session_ <<
            "INSERT INTO workflow_rules (workflow_id, rule_id, rule_order) "
            "VALUES (:wid, :rid, :rorder)",
            soci::use(workflow.id), soci::use(rule->id), soci::use(static_cast<int>(i));
    }

    tr.commit();
}

std::optional<Workflow> DbWorkflowRepository::findById(int id) {
    soci::rowset<soci::row> rs = ((*session_).prepare <<
        "SELECT id, description, is_active FROM workflows WHERE id = :id", soci::use(id));

    for (auto& row : rs) {
        Workflow wf;
        wf.id = row.get<int>(0);
        wf.description = row.get<std::string>(1);
        DbBool isActive;
    isActive = row.get<int>(2) != 0;
    wf.isActive = isActive;

        soci::rowset<soci::row> ruleRs = ((*session_).prepare <<
            "SELECT r.id, r.expression, r.action, r.description, r.is_active, r.priority, "
            "       r.timeout_ms, r.cache_duration_ms, r.depends_on_rule_id "
            "FROM rules r "
            "JOIN workflow_rules wr ON wr.rule_id = r.id "
            "WHERE wr.workflow_id = :wid "
            "ORDER BY wr.rule_order",
            soci::use(id));

        DbRuleRepository ruleRepo(session_);
        for (auto& rrow : ruleRs) {
            wf.rules.push_back(std::make_shared<Rule>(ruleRepo.rowToRule(rrow)));
        }

        return wf;
    }
    return std::nullopt;
}

std::vector<Workflow> DbWorkflowRepository::findAll() {
    std::vector<Workflow> workflows;
    soci::rowset<soci::row> rs = ((*session_).prepare <<
        "SELECT id FROM workflows");

    for (auto& row : rs) {
        int wid = row.get<int>(0);
        auto wf = findById(wid);
        if (wf.has_value()) {
            workflows.push_back(std::move(wf.value()));
        }
    }
    return workflows;
}

void DbWorkflowRepository::remove(int id) {
    *session_ << "DELETE FROM workflow_rules WHERE workflow_id = :id", soci::use(id);
    *session_ << "DELETE FROM workflows WHERE id = :id", soci::use(id);
}

bool DbWorkflowRepository::exists(int id) {
    int count = 0;
    *session_ << "SELECT COUNT(*) FROM workflows WHERE id = :id", soci::into(count), soci::use(id);
    return count > 0;
}

size_t DbWorkflowRepository::count() {
    size_t count = 0;
    *session_ << "SELECT COUNT(*) FROM workflows", soci::into(count);
    return count;
}

void DbWorkflowRepository::clear() {
    *session_ << "DELETE FROM workflow_rules";
    *session_ << "DELETE FROM workflows";
}

// ============================================================================
// DbVersionRepository (stub — requires schema alignment with RuleVersion)
// ============================================================================

DbVersionRepository::DbVersionRepository(std::shared_ptr<soci::session> session)
    : session_(std::move(session)) {
    createSchema();
}

void DbVersionRepository::createSchema() {
    *session_ <<
        "CREATE TABLE IF NOT EXISTS rule_versions ("
        "  version_id VARCHAR(255),"
        "  rule_id INTEGER,"
        "  expression TEXT,"
        "  action TEXT,"
        "  priority INT,"
        "  is_active BOOLEAN,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  author VARCHAR(255),"
        "  change_summary TEXT,"
        "  parent_version_id VARCHAR(255),"
        "  PRIMARY KEY (version_id, rule_id)"
        ")";
}

void DbVersionRepository::saveVersion(const RuleVersion& version, int ruleId) {
    DbBool isActive = version.isActive;
    *session_ <<
        "INSERT INTO rule_versions (version_id, rule_id, expression, action, priority, is_active, author, change_summary, parent_version_id) "
        "VALUES (:vid, :rid, :expr, :act, :prio, :active, :author, :summary, :parent)",
        soci::use(version.versionId), soci::use(ruleId), soci::use(version.expression),
        soci::use(version.action), soci::use(version.priority), soci::use(isActive),
        soci::use(version.author), soci::use(version.changeSummary), soci::use(version.parentVersionId);
}

std::vector<RuleVersion> DbVersionRepository::findVersionsForRule(int ruleId) {
    std::vector<RuleVersion> versions;
    soci::rowset<soci::row> rs = ((*session_).prepare <<
        "SELECT version_id, expression, action, priority, is_active, created_at, author, change_summary, parent_version_id "
        "FROM rule_versions WHERE rule_id = :rid ORDER BY created_at DESC",
        soci::use(ruleId));

    for (auto& row : rs) {
        RuleVersion rv;
        rv.versionId = row.get<std::string>(0);
        rv.ruleId = std::to_string(ruleId);
        rv.expression = row.get<std::string>(1);
        rv.action = row.get<std::string>(2);
        rv.priority = row.get<int>(3);
        rv.isActive = row.get<int>(4) != 0;
        rv.author = row.get<std::string>(6);
        rv.changeSummary = row.get<std::string>(7);
        rv.parentVersionId = row.get<std::string>(8);
        versions.push_back(rv);
    }
    return versions;
}

std::optional<RuleVersion> DbVersionRepository::findVersion(int ruleId, const std::string& versionId) {
    soci::rowset<soci::row> rs = ((*session_).prepare <<
        "SELECT version_id, expression, action, priority, is_active, created_at, author, change_summary, parent_version_id "
        "FROM rule_versions WHERE rule_id = :rid AND version_id = :vid",
        soci::use(ruleId), soci::use(versionId));
    
    for (auto& row : rs) {
        RuleVersion rv;
        rv.versionId = row.get<std::string>(0);
        rv.ruleId = std::to_string(ruleId);
        rv.expression = row.get<std::string>(1);
        rv.action = row.get<std::string>(2);
        rv.priority = row.get<int>(3);
        rv.isActive = row.get<int>(4) != 0;
        rv.author = row.get<std::string>(6);
        rv.changeSummary = row.get<std::string>(7);
        rv.parentVersionId = row.get<std::string>(8);
        return rv;
    }
    return std::nullopt;
}

void DbVersionRepository::removeAllVersionsForRule(int ruleId) {
    *session_ << "DELETE FROM rule_versions WHERE rule_id = :rid", soci::use(ruleId);
}

// ============================================================================
// DbConnectionFactory
// ============================================================================

std::shared_ptr<soci::session> DbConnectionFactory::create(
    const std::string& backend,
    const std::string& connectionString) {
    return std::make_shared<soci::session>(backend, connectionString);
}

void DbConnectionFactory::initializeSchema(const std::shared_ptr<soci::session>& session) {
    DbRuleRepository ruleRepo(session);
    DbWorkflowRepository wfRepo(session);
    DbVersionRepository verRepo(session);
}

} // namespace ext
} // namespace fastrules
