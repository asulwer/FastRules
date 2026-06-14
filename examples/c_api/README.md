# FastRules C API

This directory contains the **C-compatible API** for FastRules, enabling interoperability with other languages via FFI (Foreign Function Interface).

## Files

- `fastrules_c_api.h` - C API header with function declarations
- `fastrules_c_api.cpp` - C API implementation

## Supported Languages

The C API is used by the following language bindings:

| Language | Example Location | Binding Method |
|----------|------------------|----------------|
| C# | `examples/csharp_example/` | P/Invoke |
| Python | `examples/python_example/` | ctypes |

## Building

### Windows (Visual Studio)

```cmd
cl /LD /O2 /I../../include /I. \
   fastrules_c_api.cpp \
   /link /OUT:fastrules_c_api.dll \
   ../../build_vs/Release/fastrules.lib
```

### Linux / macOS

```bash
g++ -shared -fPIC -O3 -I../../include -I. \
    -o libfastrules_c_api.so \
    fastrules_c_api.cpp \
    -L../../build -lfastrules
```

### CMake (Recommended)

Add to your CMakeLists.txt:

```cmake
add_library(fastrules_c_api SHARED
    fastrules_c_api.cpp
)

target_link_libraries(fastrules_c_api PRIVATE fastrules)
```

## API Overview

### Engine Management

```c
fastrules_engine_t fastrules_engine_create(void);
void fastrules_engine_destroy(fastrules_engine_t engine);
const char* fastrules_engine_get_last_error(fastrules_engine_t engine);
```

### Workflow Creation (In-Memory)

```c
fastrules_workflow_t fastrules_workflow_create(
    fastrules_engine_t engine,
    int id,
    const char* description
);

fastrules_error_t fastrules_workflow_add_rule(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    int id,
    const char* expression,
    const char* action,
    const char* description,
    bool isActive
);
```

### Execution

```c
fastrules_error_t fastrules_workflow_compile(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow
);

fastrules_error_t fastrules_workflow_execute(
    fastrules_engine_t engine,
    fastrules_workflow_t workflow,
    const char* json_params,
    char** results
);
```

## Design Notes

- The C API provides a **simplified interface** to the C++ FastRules library
- **In-memory workflow creation** - no JSON required
- **Thread-safe** - each engine instance is independent
- **Error handling** via error codes and `get_last_error()`

## Extending

To add support for a new language:

1. Include `fastrules_c_api.h` in your binding
2. Load the shared library (`fastrules_c_api.dll` / `libfastrules_c_api.so`)
3. Call the C API functions via your language's FFI mechanism

See the C# and Python examples for reference implementations.
