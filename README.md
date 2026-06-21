# FastRules

[![CI](https://github.com/asulwer/fastrules/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/asulwer/fastrules/actions/workflows/ci.yml)
[![Coverage](https://codecov.io/gh/asulwer/fastrules/branch/main/graph/badge.svg)](https://codecov.io/gh/asulwer/fastrules)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://asulwer.github.io/FastRules)

High-performance C++23 business rules engine with Lua expressions. Core library is dependency-light; persistence extensions add JSON, XML, or database support as needed.

> A C++ rewrite of [RoslynRules](https://github.com/asulwer/roslynrules) with Lua instead of C#/Roslyn compilation.

## Why FastRules?

| | [RoslynRules](https://github.com/asulwer/roslynrules) | FastRules |
|---|---|---|
| **Language** | C# | C++23 |
| **Expression engine** | Roslyn compiler → IL | Lua → bytecode |
| **Compile time** | ~50ms first, cached after | ~1ms |
| **Memory** | ~50MB | ~2MB |
| **Deployment** | .NET runtime required | Self-contained |
| **Sandboxing** | Assembly whitelist | Lua environment removal |

## Architecture

FastRules has a **minimal core** plus **optional persistence extensions**:

```
┌─────────────────────────────────────────┐
│         fastrules (core)                │
│  • Lua expressions + actions            │
│  • Rules, workflows, dependencies       │
│  • C++ type registration                │
│  • Parallel / streaming execution       │
│  • Caching, timeouts, tracing           │
└─────────────────────────────────────────┘
                    │
    ┌───────────────┼───────────────┐
    ▼               ▼               ▼
┌─────────┐   ┌─────────┐   ┌─────────────┐
│fastrules│   │fastrules│   │ fastrules   │
│-json    │   │-xml     │   │ -db         │
│         │   │         │   │             │
│JSON load│   │XML load │   │SOCI-based   │
│+ save   │   │+ save   │   │PostgreSQL   │
└─────────┘   └─────────┘   │MySQL, etc.  │
                            └─────────────┘
```

**Core** has zero JSON/XML/DB dependencies. Add only the extensions you need.

## Quick Start (Decision Tree)

```
┌─────────────────────────────────────────────────────┐
│  What do you need?                                  │
└─────────────────────────────────────────────────────┘
    │
    ├─► Just rules in code ────────► Core only
    │                                  cmake -B build -S .
    │
    ├─► JSON config files ─────────► + JSON extension
    │                                  -DFASTRULES_BUILD_EXTENSIONS=ON
    │
    ├─► XML config files ──────────► + XML extension
    │                                  (included with EXTENSIONS)
    │
    ├─► Database persistence ──────► + DB extension
    │                                  -DFASTRULES_BUILD_DB=ON
    │                                  (requires SOCI via vcpkg)
    │
    └─► Maximum performance ─────────► LuaJIT backend
                                       -DFASTRULES_USE_LUAJIT=ON
```

## Features

### Core
- ✅ Lua expressions and actions
- ✅ C++20 coroutines (`co_await`)
- ✅ [Parallel execution](docs/parallel-execution.md) - executeParallel vs AsyncWorkflow guidance
- ✅ Streaming results
- ✅ Dependency chains
- ✅ Type registration (C++ structs in Lua)
- ✅ Enum registration
- ✅ Execution tracing with JSON export
- ✅ Built-in predicates (isNotNull, inRange, matchesRegex, etc.)
- ✅ Caching with TTL
- ✅ Timeout enforcement
- ✅ Security sandboxing + dangerous pattern detection
- ✅ Structured logging
- ✅ State cleanup for long-running apps
- ✅ **AOT compilation** — pre-compile workflows to binary bundles
- ✅ **Rule versioning** — semantic versioning with history and rollback

### Extensions
- ✅ **fastrules-json** — Load/save rules and workflows from JSON (nlohmann/json)
- ✅ **fastrules-xml** — Load/save rules and workflows from XML (pugixml)
- ✅ **fastrules-db** — Persist rules to PostgreSQL, MySQL, SQLite, etc. via SOCI
- ✅ **LuaJIT backend** — Optional LuaJIT instead of PUC-Rio Lua

**Extension Architecture:**

Extensions are built as separate CMake targets that link against the core `fastrules` library.
Each extension lives in `extensions/<name>/` with its own `CMakeLists.txt`, source code, and tests.

```
extensions/
├── json/          # fastrules-json target
│   ├── src/       # json_repository.cpp, json_loader.cpp
│   └── tests/     # Extension-specific tests
├── xml/           # fastrules-xml target
│   ├── src/       # xml_loader.cpp
│   └── tests/     # Extension-specific tests
└── db/            # fastrules-db target (requires SOCI)
    ├── src/
    └── examples/
```

Build with extensions:
```bash
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
```

Link your app against specific extensions:
```cmake
target_link_libraries(myapp PRIVATE fastrules fastrules-json fastrules-xml)
```

## Requirements

- CMake 3.28+
- C++23 compiler (GCC 13+, Clang 17+, MSVC 2022+)
- Git (for FetchContent dependencies)

## Quick Start

### Core Only (No Extensions)

```cpp
#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

LuaEngine engine;

// Create a rule in pure C++
Rule rule;
rule.id = 1;
rule.expression = "age >= 18";
rule.action = "eligible = true";
rule.priority = 0;

// Compile and execute
Workflow workflow;
workflow.description = "Simple validation";
workflow.rules.push_back(rule);
workflow.compile(engine);

std::vector<RuleParameter> params;
params.emplace_back("age", 25);

auto results = workflow.execute(engine, params);
// results[0].isSuccess() == true
```

### With JSON Extension

```cpp
#include <fastrules/json_loader.hpp>
#include <fastrules.hpp>

// Load workflow from JSON
auto jsonStr = readFile("rules.json");
auto workflow = fastrules::JsonLoader::loadWorkflow(jsonStr);
```

## Installation

### vcpkg

```bash
vcpkg install fastrules              # Core only
vcpkg install fastrules[extensions]  # Core + JSON + XML
vcpkg install fastrules[db]          # Core + database support
vcpkg install fastrules[luajit]      # Use LuaJIT instead of PUC-Rio Lua
vcpkg install fastrules[tests]       # Build tests
```

### Conan

```bash
conan install fastrules/0.1.0                          # Core only
conan install fastrules/0.1.0 -o build_extensions=True  # + JSON/XML
conan install fastrules/0.1.0 -o with_db=True            # + database
conan install fastrules/0.1.0 -o with_luajit=True        # Use LuaJIT
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    fastrules
    GIT_REPOSITORY https://github.com/asulwer/fastrules.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(fastrules)

target_link_libraries(your_target fastrules)

# Optional: add extensions
target_link_libraries(your_target fastrules fastrules-json)
```

### Manual CMake

```bash
# Core only
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON

# Core + JSON/XML extensions
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON

# Core + all extensions (requires SOCI installed)
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON -DFASTRULES_BUILD_DB_EXT=ON
```

## Examples

| Example | Description | Extensions Used |
|---|---|---|
| `simple_example.cpp` | Basic rule creation and execution | None (core only) |
| `core_only_example.cpp` | Comprehensive core API demo | None (core only) |
| `json_example.cpp` | Load workflow from JSON | fastrules-json |
| `json_example_auto.cpp` | Auto-discovery, no manual registration | fastrules-json |
| `json_typed_example.cpp` | Typed objects in JSON workflows | fastrules-json |
| `json_workflow_example.cpp` | Full JSON workflow with primitives | fastrules-json |
| `loop_example.cpp` | Register once, execute in loop | fastrules-json |
| `xml_example.cpp` | Load/save rules from XML | fastrules-xml |
| `db_example.cpp` | Database persistence with SOCI | fastrules-db |

## Documentation

Full documentation is available on [GitHub Pages](https://asulwer.github.io/FastRules).

## Building

```powershell
# Configure with all extensions
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON -DFASTRULES_BUILD_EXAMPLES=ON -DFASTRULES_BUILD_EXTENSIONS=ON

# Build
cmake --build build --config Release

# Test (run doctest binaries directly)
build\Release\fastrules_tests.exe
build\Release\fastrules-json-tests.exe
build\Release\fastrules-xml-tests.exe
build\Release\fastrules-db-tests.exe
```
On Windows the extension test executables copy `fastrules.dll` next to themselves
at build time. The DB test still needs `SOCI_BACKENDS_PATH` pointed at its
output directory, e.g.:

```powershell
$env:SOCI_BACKENDS_PATH = 'build\extensions\db\tests\Release'
build\extensions\db\tests\Release\fastrules-db-tests.exe
```

## Example Rule JSON

```json
{
  "id": 1,
  "version": "1.0.0",
  "rules": [
    {
      "id": 1,
      "expression": "customer.age >= 18",
      "action": "eligible = true",
      "timeout": 100,
      "priority": 0
    }
  ]
}
```

## License

MIT
