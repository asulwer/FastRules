/**
 * FastRules C API Header
 * 
 * This header defines a C-compatible API for FastRules to enable
 * interoperability with other languages (Python, C#, etc.) via FFI.
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
    FASTRULES_ERROR_INVALID_JSON = -2,
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
fastrules_engine_t fastrules_engine_create(void);

/**
 * Destroy a FastRules engine.
 * @param engine Engine handle
 */
void fastrules_engine_destroy(fastrules_engine_t engine);

/**
 * Get the last error message.
 * @param engine Engine handle
 * @return Error string (owned by engine, do not free)
 */
const char* fastrules_engine_get_last_error(fastrules_engine_t engine);

// ============================================================================
// Workflow Management
// ============================================================================

/**
 * Create a workflow from JSON.
 * @param engine Engine handle
 * @param json JSON string defining workflow and rules
 * @return Workflow handle or NULL on error
 */
fastrules_workflow_t fastrules_workflow_create_from_json(
    fastrules_engine_t engine,
    const char* json
);

/**
 * Destroy a workflow.
 * @param workflow Workflow handle
 */
void fastrules_workflow_destroy(fastrules_workflow_t workflow);

/**
 * Compile a workflow (prepares Lua code).
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @return Error code
 */
fastrules_error_t fastrules_workflow_compile(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow
);

// ============================================================================
// Execution
// ============================================================================

/**
 * Execute a workflow with parameters.
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param json_params JSON object with parameter values
 * @param results Output: JSON array of results (caller must free)
 * @return Error code
 */
fastrules_error_t fastrules_workflow_execute(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* json_params,
    char** results
);

/**
 * Execute a workflow in parallel.
 * @param engine Engine handle
 * @param workflow Workflow handle
 * @param json_params JSON object with parameter values
 * @param results Output: JSON array of results (caller must free)
 * @return Error code
 */
fastrules_error_t fastrules_workflow_execute_parallel(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* json_params,
    char** results
);

/**
 * Free memory allocated by FastRules (for result strings).
 * @param ptr Pointer to free
 */
void fastrules_free(char* ptr);

// ============================================================================
// Utility
// ============================================================================

/**
 * Get FastRules version string.
 * @return Version string (static, do not free)
 */
const char* fastrules_get_version(void);

/**
 * Check if a JSON string is valid workflow definition.
 * @param json JSON string to validate
 * @return true if valid
 */
bool fastrules_validate_workflow_json(const char* json);

#ifdef __cplusplus
}
#endif

#endif // FASTRULES_C_API_H
