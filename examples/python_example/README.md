# FastRules Python Example

This example demonstrates how to use FastRules from Python via a C API wrapper.

## Prerequisites

1. **Build FastRules as a shared library:**
   ```bash
   cmake -B build -S . \
     -DFASTRULES_BUILD_SHARED=ON \
     -DFASTRULES_BUILD_EXTENSIONS=ON
   cmake --build build --config Release
   ```

2. **Python 3.7+**

3. **Build the C API wrapper:**
   ```bash
   # Compile the C API wrapper as a shared library
   # On Windows (MSVC):
   cl /LD /O2 /I../../include /I../../extensions/fastrules-json/include \
      /I_deps/json-src/single_include /I_deps/lua-src/include \
      fastrules_c_api.cpp \
      /link /OUT:fastrules.dll \
      ../../build/Release/fastrules.lib
   
   # On Linux:
   g++ -shared -fPIC -O3 -o libfastrules.so fastrules_c_api.cpp \
       -L../../build -lfastrules -L../../extensions/fastrules-json \
       -lfastrules-json
   ```

## Usage

```bash
python fastrules_example.py
```

## Architecture

```
fastrules_example.py
       |
       v
   [ctypes]
       |
       v
fastrules_c_api.h / fastrules_c_api.cpp (C wrapper)
       |
       v
   FastRules C++ Library
       |
       v
   Lua Engine
```

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

4. **Documentation**:
   - Sphinx docs
   - Jupyter notebook examples

## Example Code

```python
from fastrules import FastRulesEngine

# Create engine
engine = FastRulesEngine()

# Load workflow from JSON
workflow = engine.load_workflow("""
{
    "id": 1,
    "rules": [
        {"id": 1, "expression": "age >= 18"}
    ]
}
""")

# Compile and execute
workflow.compile()
results = workflow.execute({"age": 25})

for result in results:
    print(f"Rule {result.rule_id}: {result.success}")
```
