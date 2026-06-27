---
layout: default
title: Home
nav_order: 0
permalink: /
has_children: false
---

# FastRules

High-performance C++23 business rules engine with Lua expressions. Core library is dependency-light; persistence extensions add JSON, XML, or database support as needed.

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

## Documentation Map

This site documents the full FastRules API and feature set. The
[README](https://github.com/asulwer/fastrules#readme) is a short getting-started
showcase; everything else is documented here.

### Start here
- [Getting Started](getting_started.md) — install, build options, your first rule
- [Architecture](architecture.md) — core design and component overview
- [Core Concepts](concepts.md) — rules, workflows, contexts, results
- [Examples](examples.md) — guide to the runnable examples
- [Migration from RoslynRules](migration.md)

### API Reference
- [Rule](api/rule.md)
- [Workflow](api/workflow.md)
- [LuaEngine](api/lua_engine.md)
- [TypeRegistry](api/type_registry.md)
- [ActionCallbacks](api/action_callbacks.md)
- [AsyncWorkflow](api/async_workflow.md)

### Features
- [Predicates](predicates.md) — built-in predicate factories
- [Parallel Execution](parallel-execution.md) — `executeParallel` vs `AsyncWorkflow`
- [Adaptive Execution](adaptive-execution.md)
- [Security](security.md) — sandboxing and expression validation
- [Observability](observability.md) — tracing and performance counters
- [Logging](logging.md)
- [Performance](performance.md)
- [Lua Compatibility](lua-compatibility.md)
- [Troubleshooting](troubleshooting.md)

### Advanced
- [AOT Compilation & Versioning](advanced/aot-and-versioning.md)
- [Custom Methods](advanced/custom-methods.md)

### Extensions
- [JSON](extensions/json.md)
- [XML](extensions/xml.md)
- [Database](extensions/db.md)

### Embedding & Project
- [C API](c_api.md) — embedding from C, C#, and Python
- [Contributing](contributing.md)
- [Coverage](coverage.md)
- [Changelog](changelog.md)

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
conan install fastrules/0.2.0                          # Core only
conan install fastrules/0.2.0 -o build_extensions=True  # + JSON/XML
conan install fastrules/0.2.0 -o with_db=True            # + database
conan install fastrules/0.2.0 -o with_luajit=True        # Use LuaJIT
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    fastrules
    GIT_REPOSITORY https://github.com/asulwer/fastrules.git
    GIT_TAG v0.2.0
)
FetchContent_MakeAvailable(fastrules)

target_link_libraries(your_target fastrules)

# Optional: add extensions
target_link_libraries(your_target fastrules fastrules-json)
```

## Quick Start

### Core Only (No Extensions)

```cpp
#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

LuaEngine engine;

// Create a rule in pure C++
auto rule = Rule::create(1, "age >= 18");
rule.withAction("eligible = true");

// Compile and execute
Workflow workflow;
workflow.id = 1;
workflow.description = "Simple validation";
workflow.rules.push_back(rule.build());
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

### With XML Extension

```cpp
#include <fastrules/xml_loader.hpp>
#include <fastrules.hpp>

// Load workflow from XML
auto xmlStr = readFile("rules.xml");
auto workflow = fastrules::XmlLoader::loadWorkflow(xmlStr);
```

### With Database Extension

```cpp
#include <fastrules/db_repository.hpp>
#include <fastrules.hpp>

// Load workflow from database
fastrules::DbRepository repo("connection_string");
auto workflow = repo.loadWorkflow("workflow-1");
```

## License

MIT
