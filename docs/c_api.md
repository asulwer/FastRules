---
layout: default
title: C API
nav_order: 6
parent: API
---

# C API

FastRules provides a C API for interoperability with other languages (Python, C#, etc.) via FFI. The C API is built into the core library when `FASTRULES_BUILD_SHARED` is enabled.

## Building with C API

### CMake Option

Enable the shared library build with C API exports:

```bash
cmake -B build -S . -DFASTRULES_BUILD_SHARED=ON -DFASTRULES_BUILD_C_API=ON
cmake --build build --config Release
```

### Build Script

Use the provided PowerShell script:

```powershell
.\build.ps1 -Configuration Release
```

This automatically enables `FASTRULES_BUILD_SHARED` and `FASTRULES_BUILD_C_API`, generating:
- `fastrules.dll` - Core library with C API exports
- `fastrules.lib` - Import library for linking

## Using the C API

### C# / P/Invoke

```csharp
using System;
using System.Runtime.InteropServices;

public class FastRules
{
    private const string DllName = "fastrules.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr fastrules_engine_create();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void fastrules_engine_destroy(IntPtr engine);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr fastrules_workflow_create(IntPtr engine, int id, string description);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int fastrules_workflow_add_rule(
        IntPtr engine, IntPtr workflow, int id, string name,
        string expression, string action, string description, bool isActive);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int fastrules_workflow_compile(IntPtr engine, IntPtr workflow);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int fastrules_workflow_execute(
        IntPtr engine, IntPtr workflow, string params_str, out IntPtr results);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void fastrules_free(IntPtr ptr);
}
```

### Python / ctypes

```python
import ctypes
from ctypes import c_char_p, c_int, c_bool, c_void_p, POINTER

# Load the DLL
fastrules = ctypes.CDLL('./fastrules.dll')

# Define function signatures
fastrules.fastrules_engine_create.restype = c_void_p
fastrules.fastrules_engine_destroy.argtypes = [c_void_p]

fastrules.fastrules_workflow_create.argtypes = [c_void_p, c_int, c_char_p]
fastrules.fastrules_workflow_create.restype = c_void_p

fastrules.fastrules_workflow_add_rule.argtypes = [
    c_void_p, c_void_p, c_int, c_char_p, c_char_p, c_char_p, c_char_p, c_bool
]
fastrules.fastrules_workflow_add_rule.restype = c_int

fastrules.fastrules_workflow_execute.argtypes = [
    c_void_p, c_void_p, c_char_p, POINTER(c_char_p)
]
fastrules.fastrules_workflow_execute.restype = c_int

fastrules.fastrules_free.argtypes = [c_char_p]

# Create engine
engine = fastrules.fastrules_engine_create()

# Create workflow
workflow = fastrules.fastrules_workflow_create(engine, 1, b"test-workflow")

# Add rule
fastrules.fastrules_workflow_add_rule(
    engine, workflow, 1, b"test-rule",
    b"age >= 18", b"eligible = true", None, True
)

# Compile
fastrules.fastrules_workflow_compile(engine, workflow)

# Execute
results = c_char_p()
fastrules.fastrules_workflow_execute(engine, workflow, b"age=25", ctypes.byref(results))
print(results.value.decode())
fastrules.fastrules_free(results)

# Cleanup
fastrules.fastrules_workflow_destroy(workflow)
fastrules.fastrules_engine_destroy(engine)
```

## API Reference

### Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | `FASTRULES_OK` | Success |
| -1 | `FASTRULES_ERROR_NULL_PTR` | Null pointer error |
| -2 | `FASTRULES_ERROR_COMPILATION_FAILED` | Lua compilation failed |
| -3 | `FASTRULES_ERROR_EXECUTION_FAILED` | Rule execution failed |
| -4 | `FASTRULES_ERROR_MEMORY` | Memory allocation failed |
| -99 | `FASTRULES_ERROR_UNKNOWN` | Unknown error |

### Engine Management

```c
// Create engine
fastrules_engine_t fastrules_engine_create(void);

// Destroy engine
void fastrules_engine_destroy(fastrules_engine_t engine);

// Get last error message
const char* fastrules_engine_get_last_error(fastrules_engine_t engine);
```

### Workflow Creation

```c
// Create workflow
fastrules_workflow_t fastrules_workflow_create(
    fastrules_engine_t engine,
    int id,
    const char* description
);

// Add rule
fastrules_error_t fastrules_workflow_add_rule(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int id,
    const char* name,
    const char* expression,
    const char* action,
    const char* description,
    bool isActive
);

// Set rule priority
fastrules_error_t fastrules_workflow_set_rule_priority(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int rule_id,
    int priority
);

// Compile workflow
fastrules_error_t fastrules_workflow_compile(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow
);

// Destroy workflow
void fastrules_workflow_destroy(fastrules_workflow_t workflow);
```

### Execution

```c
// Execute workflow
fastrules_error_t fastrules_workflow_execute(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* params_str,
    char** results
);

// Execute in parallel
fastrules_error_t fastrules_workflow_execute_parallel(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* params_str,
    char** results
);

// Free result memory
void fastrules_free(char* ptr);
```

### Complex Types

```c
// Register a custom type
fastrules_type_t fastrules_register_type(
    fastrules_engine_t engine,
    const char* type_name,
    const char* fields
);

// Create object instance
fastrules_object_t fastrules_object_create(
    fastrules_engine_t engine,
    fastrules_type_t type
);

// Set field value
fastrules_error_t fastrules_object_set_field(
    fastrules_engine_t engine,
    fastrules_object_t obj,
    const char* field_name,
    const char* value
);

// Add object as parameter
fastrules_error_t fastrules_add_object_param(
    fastrules_engine_t engine,
    const char* existing_params,
    const char* param_name,
    fastrules_object_t obj,
    char** out_params
);

// Destroy object
void fastrules_object_destroy(
    fastrules_engine_t engine,
    fastrules_object_t obj
);
```

## Parameter Format

Parameters are passed as semicolon-separated key=value pairs:

```
"age=25;name=John;active=true;salary=50000.50"
```

Supported types:
- `int` - Integer values
- `double` - Floating point values
- `bool` - `true` or `false`
- `string` - Text values

## Result Format

Results are returned as semicolon-separated entries:

```
"rule1:1::rule2:0:age is too small"
```

Format: `name:success:error` where:
- `name` - Rule name
- `success` - 1 (passed) or 0 (failed)
- `error` - Error message (empty if passed)

## Examples

See the `examples/csharp_example/` and `examples/python_example/` directories for complete working examples.
