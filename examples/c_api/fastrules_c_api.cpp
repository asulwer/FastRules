/**
 * FastRules C API Implementation
 * 
 * This file implements the C API by wrapping the C++ FastRules classes.
 * Build as a shared library for use with Python, C#, etc.
 * 
 * Uses only FastRules core - no JSON dependency.
 */

// Define FASTRULES_C_API_BUILDING so functions are exported
#define FASTRULES_C_API_BUILDING

#include "fastrules_c_api.h"
#include <fastrules.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>

using namespace fastrules;

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

// Simple parameter parser: "key=value;key2=value2"
// Supports: int, double, bool, string
static std::vector<RuleParameter> parse_params(const char* params_str) {
    std::vector<RuleParameter> params;
    
    if (!params_str || strlen(params_str) == 0) {
        return params;
    }
    
    std::string input(params_str);
    size_t pos = 0;
    
    while (pos < input.length()) {
        // Find '='
        size_t eq_pos = input.find('=', pos);
        if (eq_pos == std::string::npos) break;
        
        std::string key = input.substr(pos, eq_pos - pos);
        
        // Find ';' or end
        size_t end_pos = input.find(';', eq_pos + 1);
        if (end_pos == std::string::npos) {
            end_pos = input.length();
        }
        
        std::string value_str = input.substr(eq_pos + 1, end_pos - eq_pos - 1);
        
        // Trim whitespace
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t");
            size_t end = s.find_last_not_of(" \t");
            if (start == std::string::npos) s = "";
            else s = s.substr(start, end - start + 1);
        };
        trim(key);
        trim(value_str);
        
        // Parse value based on content
        if (value_str == "true" || value_str == "TRUE") {
            params.emplace_back(key, true);
        } else if (value_str == "false" || value_str == "FALSE") {
            params.emplace_back(key, false);
        } else {
            // Try integer
            char* endptr;
            long ival = strtol(value_str.c_str(), &endptr, 10);
            if (*endptr == '\0') {
                params.emplace_back(key, static_cast<int>(ival));
            } else {
                // Try double
                double dval = strtod(value_str.c_str(), &endptr);
                if (*endptr == '\0') {
                    params.emplace_back(key, dval);
                } else {
                    // String
                    params.emplace_back(key, value_str);
                }
            }
        }
        
        pos = end_pos + 1;
    }
    
    return params;
}

// Format results as: "id1:success1:error1;id2:success2:error2"
// success: 1 or 0, error: optional message
static char* format_results(const std::vector<RuleResult>& results) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) oss << ";";
        
        const auto& result = results[i];
        oss << result.ruleId << ":" << (result.isSuccess() ? 1 : 0);
        
        if (result.exception.has_value()) {
            oss << ":" << result.exception->what();
        }
    }
    
    std::string str = oss.str();
    char* output = static_cast<char*>(std::malloc(str.length() + 1));
    if (output) {
        std::strcpy(output, str.c_str());
    }
    return output;
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
// Workflow Management (Legacy - deprecated, kept for compatibility)
// ============================================================================

fastrules_workflow_t fastrules_workflow_create_from_json(
    fastrules_engine_t engine,
    const char* json_str
) {
    if (!engine || !json_str) {
        if (engine) set_error(engine, "Null pointer argument");
        return nullptr;
    }
    
    set_error(engine, "JSON support removed. Use fastrules_workflow_create() and fastrules_workflow_add_rule() instead.");
    return nullptr;
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
    const char* params_str,
    char** results
) {
    if (!engine || !workflow || !results) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        // Parse parameters (format: "key=value;key2=value2")
        auto params = parse_params(params_str);
        
        // Execute
        auto results_vec = workflow->workflow->execute(*engine->engine, params);
        
        // Format results
        *results = format_results(results_vec);
        if (!*results) {
            return FASTRULES_ERROR_MEMORY;
        }
        
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_EXECUTION_FAILED;
    }
}

fastrules_error_t fastrules_workflow_execute_parallel(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* params_str,
    char** results
) {
    if (!engine || !workflow || !results) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        // Parse parameters
        auto params = parse_params(params_str);
        
        // Execute
        auto results_vec = workflow->workflow->executeParallel(*engine->engine, params);
        
        // Format results
        *results = format_results(results_vec);
        if (!*results) {
            return FASTRULES_ERROR_MEMORY;
        }
        
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
    return "1.0.0";
}

bool fastrules_validate_workflow_json(const char* json_str) {
    // JSON validation removed - use in-memory workflow creation instead
    return false;
}

} // extern "C"
