# FastRules Python Example

This example demonstrates how to use FastRules from Python via a C API wrapper.

## Prerequisites

1. **Build FastRules C API as a shared library:**
   
   The C API source files are located in `examples/c_api/`:
   - `fastrules_c_api.h` - C API header
   - `fastrules_c_api.cpp` - C API implementation

   ```bash
   # Build using CMake (recommended)
   cmake -B build -S . \
     -DFASTRULES_BUILD_SHARED=ON \
     -DFASTRULES_BUILD_C_API=ON
   cmake --build build --config Release
   ```

   Or compile manually:
   ```bash
   # On Windows (MSVC):
   cl /LD /O2 /I../../include /I../c_api \
      ../c_api/fastrules_c_api.cpp \
      /link /OUT:fastrules_c_api.dll \
      ../../build/Release/fastrules.lib
   
   # On Linux:
   g++ -shared -fPIC -O3 -I../../include -I../c_api \
       -o libfastrules_c_api.so \
       ../c_api/fastrules_c_api.cpp \
       -L../../build -lfastrules
   ```

2. **Python 3.7+**

## Usage

```bash
python fastrules_example.py
```

## Architecture

```
fastrules_example.py (Python)
       |
       v
   [ctypes]
       |
       v
fastrules_c_api.dll / libfastrules_c_api.so (C API)
       |
       v
   FastRules C++ Library
       |
       v
   Lua Engine
```

## C API Location

The C API is centralized in `examples/c_api/` and used by:
- **C# Example** (`examples/csharp_example/`) - via P/Invoke
- **Python Example** (`examples/python_example/`) - via ctypes

This allows both languages to share the same C-compatible interface.

## For Production Use

Consider these improvements:

1. **Use pybind11** instead of ctypes for better C++ integration:
   - Automatic type conversion
   - Better error handling
   - More Pythonic API

2. **Create a proper Python package**:
   - `setup.py` or `pyproject.toml`
   - Wheels for different platforms
   - CI/CD for automated builds

3. **Add type hints**:
   - Full type stub files (.pyi)
   - IDE autocomplete support

## Example Code

```python
from fastrules import FastRulesEngine

# Create engine
engine = FastRulesEngine()

# Create workflow in-memory (no JSON required)
workflow = engine.create_workflow(1, "Customer Validation")

# Add rules programmatically
workflow.add_rule(1, "age >= 18", description="Age check")
workflow.add_rule(2, "len(name) > 0", description="Name check")

# Compile and execute
workflow.compile()
results = workflow.execute({"age": 25, "name": "Alice"})

for result in results:
    print(f"Rule {result.rule_id}: {'PASS' if result.success else 'FAIL'}")
```
