---
layout: default
title: Home
nav_order: 0
permalink: /
---

# FastRules

High-performance C++23 business rules engine with Lua expressions. Core library is dependency-light; persistence extensions add JSON, XML, or database support as needed.

> A C++ rewrite of [RoslynRules](https://github.com/asulwer/roslynrules) with Lua instead of C#/Roslyn compilation.

## Why FastRules?

| | RoslynRules | FastRules |
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

## Features

### Core
- ✅ Lua expressions and actions
- ✅ C++20 coroutines (`co_await`)
- ✅ Parallel execution
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

## Quick Start

### Core Only (No Extensions)

```cpp
#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

LuaEngine engine;

// Create a rule in pure C++
Rule rule;
rule.id = "adult-check";
rule.expression = "age >= 18";
rule.action = "eligible = true";
rule.priority = 0;

// Compile and execute
Workflow workflow;
workflow.description = "Simple validation";
workflow.rules.push_back(rule);
workflow.compile(engine);

std::vector<RuleParameter> params;
params.emplace_back("age", "int", std::any(25));

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

## License

MIT
