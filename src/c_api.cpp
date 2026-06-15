/**
 * FastRules C API Implementation
 * 
 * This file implements the C API by wrapping the C++ FastRules classes.
 * Build as a shared library for use with Python, C#, etc.
 * 
 * Uses only FastRules core - no JSON dependency.
 */

// FASTRULES_C_API_BUILDING is defined via compiler flags when building the DLL
// Only define it here if not already defined
#ifndef FASTRULES_C_API_BUILDING
#define FASTRULES_C_API_BUILDING
#endif

#include <fastrules/c_api.h>
#include <fastrules.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <map>
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

// Format results as: "id:name:success:error;id:name:success:error"
// id: rule ID, name: rule name, success: 1 or 0, error: optional message
static char* format_results(const std::vector<RuleResult>& results) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) oss << ";";
        
        const auto& result = results[i];
        
        // Include rule ID and name
        oss << result.ruleId << ":";
        std::string rule_name = result.ruleName.empty() ? "" : result.ruleName;
        oss << rule_name << ":";
        
        oss << (result.isSuccess() ? 1 : 0);
        
        if (result.exception.has_value()) {
            oss << ":" << result.exception->what();
        }
    }
    
    std::string str = oss.str();
    char* output = static_cast<char*>(std::malloc(str.length() + 1));
    if (output) {
#ifdef _WIN32
        strcpy_s(output, str.length() + 1, str.c_str());
#else
        std::strcpy(output, str.c_str());
#endif
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
    const char* name,
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
        if (name) {
            rule->name = name;
        } else {
            // Generate a name based on ID if not provided
            rule->name = "rule_" + std::to_string(id);
        }
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
// Type Registration (for complex objects like Customer)
// ============================================================================
// Type Registration (for complex objects like Customer)
// ============================================================================

// Store registered types per engine
static std::map<fastrules_engine_t, std::map<std::string, std::vector<std::pair<std::string, std::string>>>> g_registered_types;

fastrules_error_t fastrules_engine_register_type(
    fastrules_engine_t engine,
    const char* type_name,
    const char* fields
) {
    if (!engine || !type_name || !fields) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        std::vector<std::pair<std::string, std::string>> field_list;
        
        // Parse fields: "field1:type1;field2:type2"
        std::string fields_str(fields);
        size_t pos = 0;
        while (pos < fields_str.length()) {
            size_t sep = fields_str.find(';', pos);
            if (sep == std::string::npos) sep = fields_str.length();
            
            std::string field_def = fields_str.substr(pos, sep - pos);
            size_t colon = field_def.find(':');
            if (colon != std::string::npos) {
                std::string field_name = field_def.substr(0, colon);
                std::string field_type = field_def.substr(colon + 1);
                field_list.emplace_back(field_name, field_type);
            }
            pos = sep + 1;
        }
        
        // Store in global registry
        g_registered_types[engine][type_name] = field_list;
        
        // Also register with FastRules type system
        // This uses the C++ API to register the type
        // For now, we store it and use it when creating parameters
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_UNKNOWN;
    }
}

fastrules_error_t fastrules_add_typed_param(
    fastrules_engine_t engine,
    const char* params,
    const char* name,
    const char* type_name,
    const char* fields_values,
    char** out_params
) {
    if (!engine || !name || !type_name || !fields_values || !out_params) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        // Check if type is registered
        auto it = g_registered_types.find(engine);
        if (it == g_registered_types.end() || it->second.find(type_name) == it->second.end()) {
            set_error(engine, "Type not registered");
            return FASTRULES_ERROR_UNKNOWN;
        }
        
        // Build parameter string: "customer.age=25;customer.name=Alice"
        std::ostringstream oss;
        
        // Parse existing params
        std::string existing(params ? params : "");
        if (!existing.empty()) {
            oss << existing;
        }
        
        // Parse field values: "age=25;name=Alice"
        std::string fv_str(fields_values);
        size_t pos = 0;
        while (pos < fv_str.length()) {
            size_t sep = fv_str.find(';', pos);
            if (sep == std::string::npos) sep = fv_str.length();
            
            std::string fv = fv_str.substr(pos, sep - pos);
            size_t eq = fv.find('=');
            if (eq != std::string::npos) {
                std::string field = fv.substr(0, eq);
                std::string value = fv.substr(eq + 1);
                
                if (oss.tellp() > 0) oss << ";";
                oss << name << "." << field << "=" << value;
            }
            pos = sep + 1;
        }
        
        std::string result = oss.str();
        *out_params = static_cast<char*>(std::malloc(result.length() + 1));
        if (*out_params) {
#ifdef _WIN32
            strcpy_s(*out_params, result.length() + 1, result.c_str());
#else
            std::strcpy(*out_params, result.c_str());
#endif
        }
        
        return FASTRULES_OK;
    } catch (const std::exception& e) {
        set_error(engine, e.what());
        return FASTRULES_ERROR_UNKNOWN;
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
// Complex Object Support
// ============================================================================

// Internal structures for complex objects
struct FastRulesType {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields; // field_name -> type
};

struct FastRulesObject {
    fastrules_type_t type;
    std::map<std::string, std::string> field_values; // field_name -> value_as_string
};

// Global type registry per engine
static std::map<fastrules_engine_t, std::map<std::string, std::shared_ptr<FastRulesType>>> g_type_registry;

fastrules_type_t fastrules_register_type(
    fastrules_engine_t engine,
    const char* type_name,
    const char* fields
) {
    if (!engine || !type_name || !fields) {
        return nullptr;
    }
    
    try {
        auto type = std::make_shared<FastRulesType>();
        type->name = type_name;
        
        // Parse fields: "field1:type1;field2:type2"
        std::string fields_str(fields);
        size_t pos = 0;
        while (pos < fields_str.length()) {
            size_t sep = fields_str.find(';', pos);
            if (sep == std::string::npos) sep = fields_str.length();
            
            std::string field_def = fields_str.substr(pos, sep - pos);
            size_t colon = field_def.find(':');
            if (colon != std::string::npos) {
                std::string field_name = field_def.substr(0, colon);
                std::string field_type = field_def.substr(colon + 1);
                type->fields.emplace_back(field_name, field_type);
            }
            pos = sep + 1;
        }
        
        // Store in global registry
        auto type_ptr = type.get();
        g_type_registry[engine][type_name] = type;
        
        return type_ptr;
    } catch (...) {
        return nullptr;
    }
}

fastrules_object_t fastrules_object_create(
    fastrules_engine_t engine,
    fastrules_type_t type
) {
    if (!engine || !type) {
        return nullptr;
    }
    
    try {
        auto obj = new FastRulesObject();
        obj->type = type;
        return obj;
    } catch (...) {
        return nullptr;
    }
}

fastrules_error_t fastrules_object_set_field(
    fastrules_engine_t engine,
    fastrules_object_t obj,
    const char* field_name,
    const char* value
) {
    (void)engine;  // Mark as unused - engine parameter for consistency
    if (!obj || !field_name || !value) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        obj->field_values[field_name] = value;
        return FASTRULES_OK;
    } catch (...) {
        return FASTRULES_ERROR_UNKNOWN;
    }
}

void fastrules_object_destroy(
    fastrules_engine_t engine,
    fastrules_object_t obj
) {
    (void)engine;  // Mark as unused - engine parameter for consistency
    delete obj;
}

fastrules_error_t fastrules_add_object_param(
    fastrules_engine_t engine,
    const char* existing_params,
    const char* param_name,
    fastrules_object_t obj,
    char** out_params
) {
    if (!engine || !param_name || !obj || !out_params) {
        return FASTRULES_ERROR_NULL_PTR;
    }
    
    try {
        // Build parameter string from object fields
        std::ostringstream oss;
        
        // Include existing params
        if (existing_params && strlen(existing_params) > 0) {
            oss << existing_params << ";";
        }
        
        // Add object fields as prefixed parameters
        // e.g., "customer" + "age" = "customer.age=25"
        for (const auto& [field_name, value] : obj->field_values) {
            if (oss.tellp() > 0) oss << ";";
            oss << param_name << "." << field_name << "=" << value;
        }
        
        std::string result = oss.str();
        *out_params = static_cast<char*>(std::malloc(result.length() + 1));
        if (*out_params) {
#ifdef _WIN32
            strcpy_s(*out_params, result.length() + 1, result.c_str());
#else
            std::strcpy(*out_params, result.c_str());
#endif
        }
        
        return FASTRULES_OK;
    } catch (...) {
        return FASTRULES_ERROR_UNKNOWN;
    }
}

// ============================================================================
// Utility
// ============================================================================

const char* fastrules_get_version(void) {
    return "1.0.0";
}

} // extern "C"
