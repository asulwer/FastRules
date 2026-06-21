// stress_helpers.hpp
// Shared workload generators for core and extension stress tests.

#pragma once

#include <fastrules/rule.hpp>
#include <fastrules/workflow.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace fastrules::stress {

inline std::string makeExpression(int index, int paramCount) {
    std::ostringstream oss;
    for (int i = 0; i < paramCount; ++i) {
        if (i > 0) oss << " and ";
        oss << "p" << i << " > " << (index + i);
    }
    return oss.str();
}

inline std::vector<RuleParameter> makeParameters(int count, int baseValue) {
    std::vector<RuleParameter> params;
    params.reserve(count);
    for (int i = 0; i < count; ++i) {
        params.emplace_back("p" + std::to_string(i), baseValue + i + 100);
    }
    return params;
}

inline std::shared_ptr<Rule> makeRule(int id, int paramCount) {
    auto rule = std::make_shared<Rule>();
    rule->id = id;
    rule->name = "r" + std::to_string(id);
    rule->expression = makeExpression(id, paramCount);
    rule->priority = id;
    rule->description = "Stress rule " + std::to_string(id);
    return rule;
}

inline Workflow makeWorkflow(int id, size_t ruleCount, size_t paramCount) {
    Workflow wf;
    wf.id = id;
    wf.name = "stress-workflow-" + std::to_string(id);
    wf.description = "Generated stress workflow";
    for (size_t i = 0; i < ruleCount; ++i) {
        wf.rules.push_back(makeRule(static_cast<int>(id * 1000 + static_cast<int>(i) + 1), static_cast<int>(paramCount)));
    }
    return wf;
}

} // namespace fastrules::stress
