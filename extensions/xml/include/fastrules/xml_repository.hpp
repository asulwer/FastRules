#pragma once

#include <fastrules/repository.hpp>
#include <pugixml.hpp>
#include <filesystem>

namespace fastrules {
namespace ext {

/**
 * XML file-based implementation of IRuleRepository.
 *
 * Stores rules in XML format:
 * <?xml version="1.0"?>
 * <rules>
 *   <rule id="rule-1" isActive="true" priority="0" timeout="100">
 *     <expression>age >= 18</expression>
 *     <action>eligible = true</action>
 *     <version>1.0.0</version>
 *     <parameters>
 *       <param>age</param>
 *     </parameters>
 *     <dependencies>
 *       <dep>other-rule</dep>
 *     </dependencies>
 *     <childRules>
 *       <child>child-1</child>
 *     </childRules>
 *   </rule>
 * </rules>
 */
class XmlRuleRepository : public IRuleRepository {
public:
    explicit XmlRuleRepository(const std::filesystem::path& filepath);
    ~XmlRuleRepository() override = default;
    
    void save(const Rule& rule) override;
    std::optional<Rule> findById(int id) override;
    std::vector<Rule> findAll() override;
    void remove(int id) override;
    bool exists(int id) override;
    size_t count() override;
    
    void flush();
    std::filesystem::path filepath() const { return filepath_; }
    
    /**
     * Export to string (useful for debugging or transmission).
     */
    std::string toString() const;

private:
    std::filesystem::path filepath_;
    pugi::xml_document doc_;
    bool dirty_ = false;
    
    void load();
    void write();
    void ruleToXml(const Rule& rule, pugi::xml_node& parent) const;
    Rule xmlToRule(const pugi::xml_node& node) const;
};

/**
 * XML file-based implementation of IWorkflowRepository.
 */
class XmlWorkflowRepository : public IWorkflowRepository {
public:
    explicit XmlWorkflowRepository(const std::filesystem::path& filepath);
    ~XmlWorkflowRepository() override = default;
    
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
    pugi::xml_document doc_;
    bool dirty_ = false;
    
    void load();
    void write();
};

} // namespace ext
} // namespace fastrules
