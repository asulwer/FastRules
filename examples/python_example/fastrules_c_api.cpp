/**
 * FastRules C API Implementation
 * 
 * This file implements the C API by wrapping the C++ FastRules classes.
 * Build as a shared library for use with Python, C#, etc.
 */

// Define FASTRULES_C_API_BUILDING so functions are exported
#define FASTRULES_C_API_BUILDING

#include "fastrules_c_api.h"
#include <fastrules.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>
#include <map>

using namespace fastrules;
using json = nlohmann::json;

// Internal state tracking
struct FastRulesEngine {
    std::unique_ptr<LuaEngine> engine;
    std::string last_error;
};

struct FastRulesWorkflow {
    std::unique_ptr<Workflow> workflow;
};

// ============================================================================
// Helper Functions
// ============================================================================

static void set_error(FastRulesEngine* engine, const char* msg) {
    if (engine) {
        engine->last_error = msg ? msg : "Unknown error";
    }
}

static json parameters_to_json(const char* json_params) {
    if (!json_params || strlen(json_params) == 0) {
        return json::object();
    }
    return json::parse(json_params);
}

static std::vector<RuleParameter> json_to_parameters(const json& j) {
    std::vector<RuleParameter> params;
    
    for (auto& [key, value] : j.items()) {
        if (value.is_number_integer()) {
            int val = value.get<int>();
            params.emplace_back(key, val);
        } else if (value.is_number_float()) {
            double val = value.get<double>();
            params.emplace_back(key, val);
        } else if (value.is_boolean()) {
            bool val = value.get<bool>();
            params.emplace_back(key, val);
        } else if (value.is_string()) {
            std::string val = value.get<std::string>();
            params.emplace_back(key, val);
        }
    }
    
    return params;
}

static json results_to_json(const std::vector<RuleResult>& results) {
    json arr = json::array();
    
    for (const auto& result : results) {
        json r;
        r["ruleId"] = result.ruleId;
        r["success"] = result.isSuccess();
        r["executedAt"] = result.executedAt.time_since_epoch().count();
        
        if (result.exception.has_value()) {
            r["error"] = result.exception->what();
        }
        
        if (result.isSuccess()) {
            if (result.value.has_value()) {
                try {
                    r["value"] = std::any_cast<std::string>(result.value.value());
                } catch (...) {
                    r["value"] = nullptr;
                }
            }
        }
        
        arr.push_back(r);
    }
    
    return arr;
}

// ============================================================================
// Engine Management
// ============================================================================

extern "C" {

fastrules_engine_t fastrules_engine_create(void) {
    try {
        auto* engine = new FastRulesEngine();
        engine->engine = std::make_unique<LuaEngine>();
        return engine;
    } catch (...) {
        return nullptr;
    }
}

void fastrules_engine_destroy(fastrules_engine_t engine) {
    delete engine;
}

const char* fastrules_engine_get_last_error(fastrules_engine_t engine) {
    if (!engine) return "Null engine pointer";
    return engine->last_error.c_str();
}

// ============================================================================
// Workflow Creation (In-Memory)
// ============================================================================

fastrules_workflow_t fastrules_workflow_create(
    fastrules_engine_t engine,
    int id,
    const char* description
) {
    if (!engine) {
        return nullptr;
    }
    
    try {
        auto* workflow = new FastRulesWorkflow();
        workflow->workflow = std::make_unique<Workflow>();
        workflow->workflow->id = id;
        if (description) {
            workflow->workflow->description = description;
        }
        return workflow;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return nullptr;
    } catch (...) {
        set_error(engine, "Unknown error creating workflow");
        return nullptr;
    }
}

fastrules_error_t fastrules_workflow_add_rule(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int id,
    const char* expression,
    const char* action,
    const char* description,
    bool isActive
) {
    if (!engine || !workflow) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    if (!expression) {
        set_error(engine, "Expression cannot be null");
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        auto rule = std::make_shared<Rule>();
        rule->id = id;
        rule->expression = expression;
        if (action) {
            rule->action = action;
        }
        if (description) {
            rule->description = description;
        }
        rule->isActive = isActive;
        
        workflow->workflow->rules.push_back(rule);
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_UNKNOWN;
    }
}

fastrules_error_t fastrules_workflow_set_rule_priority(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int rule_id,
    int priority
) {
    if (!engine || !workflow) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        for (auto& rule : workflow->workflow->rules) {
            if (rule->id == rule_id) {
                rule->priority = priority;
                return FASTRULES_OK;
            }
        }
        set_error(engine, "Rule not found");
        return FASTRULES_ERROR_UNKNOWN;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_UNKNOWN;
    }
}

// ============================================================================
// Workflow Management
// ============================================================================

fastrules_workflow_t fastrules_workflow_create_from_json(
    fastrules_engine_t engine,
    const char* json_str
) {
    if (!engine || !json_str) {
        if (engine) set_error(engine, "Null pointer argument");
        return nullptr;
    }
    
    try {
        auto* workflow = new FastRulesWorkflow();
        
        // Parse JSON
        auto j = json::parse(json_str);
        
        // Create workflow from JSON
        // This assumes the JSON extension is available
        // For core-only builds, you'd need to parse manually
        workflow->workflow = std::make_unique<Workflow>();
        workflow->workflow->id = j.value("id", 0);
        workflow->workflow->description = j.value("description", "");
        
        // Parse rules
        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& rule_json : j["rules"]) {
                auto rule = std::make_shared<Rule>();
                rule->id = rule_json.value("id", 0);
                rule->description = rule_json.value("description", "");
                rule->expression = rule_json.value("expression", "");
                rule->action = rule_json.value("action", "");
                rule->isActive = rule_json.value("isActive", true);
                
                if (rule_json.contains("priority")) {
                    rule->priority = rule_json.value("priority", 0);
                }
                
                workflow->workflow->rules.push_back(rule);
            }
        }
        
        return workflow;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return nullptr;
    } catch (...) {
        set_error(engine, "Unknown error creating workflow");
        return nullptr;
    }
}

void fastrules_workflow_destroy(fastrules_workflow_t workflow) {
    delete workflow;
}

fastrules_error_t fastrules_workflow_compile(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow
) {
    if (!engine || !workflow) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        workflow->workflow->compile(*engine->engine);
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_COMPILATION_FAILED;
    }
}

// ============================================================================
// Execution
// ============================================================================

fastrules_error_t fastrules_workflow_execute(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* json_params,
    char** results
) {
    if (!engine || !workflow || !results) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        // Parse parameters
        auto params_json = parameters_to_json(json_params);
        auto params = json_to_parameters(params_json);
        
        // Execute
        auto results_vec = workflow->workflow->execute(*engine->engine, params);
        
        // Convert to JSON
        auto results_json = results_to_json(results_vec);
        std::string results_str = results_json.dump();
        
        // Allocate and copy result string
        *results = static_cast<char*>(std::malloc(results_str.length() + 1));
        if (!*results) {
            return FASTRULES_ERROR_MEMORY;
        }
        std::strcpy(*results, results_str.c_str());
        
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_EXECUTION_FAILED;
    }
}

fastrules_error_t fastrules_workflow_execute_parallel(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* json_params,
    char** results
) {
    if (!engine || !workflow || !results) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        auto params_json = parameters_to_json(json_params);
        auto params = json_to_parameters(params_json);
        
        auto results_vec = workflow->workflow->executeParallel(*engine->engine, params);
        
        auto results_json = results_to_json(results_vec);
        std::string results_str = results_json.dump();
        
        *results = static_cast<char*>(std::malloc(results_str.length() + 1));
        if (!*results) {
            return FASTRULES_ERROR_MEMORY;
        }
        std::strcpy(*results, results_str.c_str());
        
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_EXECUTION_FAILED;
    }
}

void fastrules_free(char* ptr) {
    std::free(ptr);
}

// ============================================================================
// Utility
// ============================================================================

const char* fastrules_get_version(void) {
    return "1.0.0";  // TODO: Get from version header
}

bool fastrules_validate_workflow_json(const char* json_str) {
    if (!json_str) return false;
    
    try {
        auto j = json::parse(json_str);
        
        // Basic validation
        if (!j.contains("id")) return false;
        if (!j.contains("rules")) return false;
        if (!j["rules"].is_array()) return false;
        
        for (const auto& rule : j["rules"]) {
            if (!rule.contains("id")) return false;
            if (!rule.contains("expression")) return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

} // extern "C"
