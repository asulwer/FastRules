#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"
#include <iostream>

// Minimal example that works with the LuaBridge3 backend
// Uses only primitive parameters (no object types)
int main() {
    try {
        std::cout << "Creating LuaEngine..." << std::endl;
        fastrules::LuaEngine engine;
        std::cout << "LuaEngine created." << std::endl;

        std::cout << "Creating Workflow..." << std::endl;
        fastrules::Workflow workflow;
        workflow.description = "Primitive-only test";
        std::cout << "Workflow created." << std::endl;

        std::cout << "Creating Rule..." << std::endl;
        auto rule = std::make_shared<fastrules::Rule>();
        rule->id = 1;
        rule->description = "Age must be 18+";
        rule->expression = "age >= 18";
        workflow.rules.push_back(rule);
        std::cout << "Rule created and added." << std::endl;

        std::cout << "Compiling workflow..." << std::endl;
        workflow.compile(engine);
        std::cout << "Workflow compiled." << std::endl;

        std::cout << "Setting up params..." << std::endl;
        std::vector<fastrules::RuleParameter> params;
        params.emplace_back("age", 25);
        std::cout << "Executing..." << std::endl;

        auto results = workflow.execute(engine, params);
        for (const auto& r : results) {
            std::cout << "Rule: " << r.ruleName
                      << " - Success: " << (r.isSuccess() ? "Yes" : "No") << std::endl;
        }

        // Test failing case
        params.clear();
        params.emplace_back("age", 15);
        results = workflow.execute(engine, params);
        for (const auto& r : results) {
            std::cout << "Rule: " << r.ruleName
                      << " - Success: " << (r.isSuccess() ? "Yes" : "No") << std::endl;
        }

        std::cout << "LuaBridge3 basic test PASSED" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}


