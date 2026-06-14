/**
 * FastRules C API Header
 * 
 * This header defines a C-compatible API for FastRules to enable
 * interoperability with other languages (Python, C#, etc.) via FFI.
 * 
 * Uses only FastRules core - no JSON dependency.
 * 
 * Build with:
 *   extern "C" wrapper functions that expose the C++ API
 * 
 * Usage:
 *   - Python: ctypes or cffi
 *   - C#: P/Invoke
 *   - Other: Standard C calling convention
 */

#ifndef FASTRULES_C_API_H
#define FASTRULES_C_API_H

#ifdef _WIN32
    #ifdef FASTRULES_C_API_BUILDING
        #define FASTRULES_C_API __declspec(dllexport)
    #else
        #define FASTRULES_C_API __declspec(dllimport)
    #endif
#else
    #define FASTRULES_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Opaque handle types
typedef struct FastRulesEngine* fastrules_engine_t;
typedef struct FastRulesWorkflow* fastrules_workflow_t;
typedef struct FastRulesResult* fastrules_result_t;

// Error codes
typedef enum {
    FASTRULES_OK = 0,
    FASTRULES_ERROR_NULL_PTR = -1,
    FASTRULES_ERROR_INVALID_JSON = -2,  /* Deprecated - kept for compatibility */
    FASTRULES_ERROR_COMPILATION_FAILED = -3,
    FASTRULES_ERROR_EXECUTION_FAILED = -4,
    FASTRULES_ERROR_MEMORY = -5,
    FASTRULES_ERROR_UNKNOWN = -99
} fastrules_error_t;

// ============================================================================
// Engine Management
// ============================================================================

/**
 * Create a new FastRules engine.
 * @return Engine handle or NULL on error
 */
FASTRULES_C_API fastrules_engine_t fastrules_engine_create(void);

/**
 * Destroy a FastRules engine.
 * @param engine Engine handle
 */
FASTRULES_C_API void fastrules_engine_destroy(fastrules_engine_t engine);

/**
 * Get the last error message.
 * @param engine Engine handle
 * @return Error string (owned by engine, do not free)
 */
FASTRULES_C_API const char* fastrules_engine_get_last_error(fastrules_engine_t engine);

// ============================================================================
// Workflow Creation (In-Memory)
// ============================================================================

/**
 * Create an empty workflow.
 * @param engine Engine handle
 * @param id Workflow ID
 * @param description Workflow description (can be NULL)
 * @return Workflow handle or NULL on error
 */
FASTRULES_C_API fastrules_workflow_t fastrules_workflow_create(
    fastrules_engine_t engine,
    int id,
    const char* description
);

/**
 * Add a rule to a workflow.
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param id Rule ID
 * @param name Rule name (can be NULL)
 * @param expression Lua expression string
 * @param action Optional action (can be NULL)
 * @param description Optional description (can be NULL)
 * @param isActive Whether rule is active
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_workflow_add_rule(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int id,
    const char* name,
    const char* expression,
    const char* action,
    const char* description,
    bool isActive
);

/**
 * Set rule priority.
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param rule_id Rule ID
 * @param priority Priority value
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_workflow_set_rule_priority(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int rule_id,
    int priority
);

/**
 * Register a custom type with the engine.
 * 
 * This allows complex objects (like Customer) to be passed to rules.
 * Fields are specified as a semicolon-separated list of name:type pairs.
 * Supported types: int, double, bool, string
 * 
 * Example: fastrules_engine_register_type(engine, "Customer", "age:int;name:string;active:bool");
 * 
 * @param engine Engine handle
 * @param type_name Name of the type (e.g., "Customer")
 * @param fields Field definitions (e.g., "age:int;name:string")
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_engine_register_type(
    fastrules_engine_t engine,
    const char* type_name,
    const char* fields
);

/**
 * Add a typed parameter to a parameter set (for complex objects).
 * 
 * @param engine Engine handle
 * @param params JSON array string of existing params (can be empty array "[]")
 * @param name Parameter name
 * @param type_name Type name (must be registered first)
 * @param fields_values Field values as "field1=value1;field2=value2"
 * @param out_params Output: new JSON array string (caller must free)
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_add_typed_param(
    fastrules_engine_t engine,
    const char* params,
    const char* name,
    const char* type_name,
    const char* fields_values,
    char** out_params
);

// ============================================================================
// Workflow Management
// ============================================================================

/**
 * Create a workflow from JSON.
 * @deprecated Use fastrules_workflow_create() and fastrules_workflow_add_rule() instead.
 *             This function now returns NULL and sets an error message.
 * @param engine Engine handle
 * @param json JSON string defining workflow and rules
 * @return Workflow handle or NULL on error
 */
FASTRULES_C_API fastrules_workflow_t fastrules_workflow_create_from_json(
    fastrules_engine_t engine,
    const char* json
);

/**
 * Destroy a workflow.
 * @param workflow Workflow handle
 */
FASTRULES_C_API void fastrules_workflow_destroy(fastrules_workflow_t workflow);

/**
 * Compile a workflow (prepares Lua code).
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_workflow_compile(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow
);

// ============================================================================
// Execution
// ============================================================================

/**
 * Execute a workflow with parameters.
 * 
 * Parameters format: "key=value;key2=value2"
 * Supported types: int, double, bool (true/false), string
 * 
 * Results format: "name:success:error;name:success:error"
 * - name: rule name
 * - success: 1 or 0
 * - error: optional error message
 *
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param params_str Parameter string (format: "key=value;key2=value2")
 * @param results Output: Result string (format: "name:success:error;name:success:error")
 *                  Caller must free with fastrules_free()
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_workflow_execute(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* params_str,
    char** results
);

/**
 * Execute a workflow in parallel.
 * 
 * Parameters format: "key=value;key2=value2"
 * Supported types: int, double, bool (true/false), string
 * 
 * Results format: "name:success:error;name:success:error"
 * - name: rule name
 * - success: 1 or 0  
 * - error: optional error message
 *
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param params_str Parameter string (format: "key=value;key2=value2")
 * @param results Output: Result string (format: "name:success:error;name:success:error")
 *                  Caller must free with fastrules_free()
 * @return Error code
 */
FASTRULES_C_API fastrules_error_t fastrules_workflow_execute_parallel(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* params_str,
    char** results
);

/**
 * Free memory allocated by FastRules (for result strings).
 * @param ptr Pointer to free
 */
FASTRULES_C_API void fastrules_free(char* ptr);

// ============================================================================
// Utility
// ============================================================================

/**
 * Get FastRules version string.
 * @return Version string (static, do not free)
 */
FASTRULES_C_API const char* fastrules_get_version(void);

/**
 * Check if a JSON string is valid workflow definition.
 * @deprecated JSON support removed. Always returns false.
 * @param json JSON string to validate
 * @return false (JSON support removed)
 */
FASTRULES_C_API bool fastrules_validate_workflow_json(const char* json);

#ifdef __cplusplus
}
#endif

#endif // FASTRULES_C_API_H
