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
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param params_str Parameter string (format: "key=value;key2=value2")
 * @param results Output: Result string (format: "id1:success1:error1;id2:success2:error2")
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
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param params_str Parameter string (format: "key=value;key2=value2")
 * @param results Output: Result string (format: "id1:success1:error1;id2:success2:error2")
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
