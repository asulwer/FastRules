---
layout: default
title: Architecture
nav_order: 3
---

# FastRules Architecture

## Core Design

FastRules replaces RoslynRules' Roslyn C# compilation with **embedded Lua** for expressions and actions. This dramatically reduces complexity while maintaining sufficient performance for most business rules scenarios.

## Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      fastrules Core                          │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                    Workflow                          │  │
│  │  resolveExecutionOrder() ──▶ topological sort      │  │
│  │  execute() ──▶ sequential per dependency level     │  │
│  │  executeParallel() ──▶ parallel per level          │  │
│  │  executeStreaming() ──▶ yield results as ready   │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                    Rule                              │  │
│  │  id, expression, action, priority, timeout          │  │
│  │  isActive, parameterNames, dependsOnRuleId            │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                   LuaEngine                          │  │
│  │  LuaState* ──▶ compileExpr(), compileAct()         │  │
│  │  registerType<T>() ──▶ bind C++ structs to Lua    │  │
│  │  registerEnum<T>() ──▶ bind C++ enums to Lua       │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                  RuleContext                         │  │
│  │  Thread-safe result storage (shared_mutex)           │  │
│  │  Metrics tracking, execution tracing                 │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
  ┌───────────┐       ┌───────────┐       ┌─────────────┐
  │ fastrules │       │ fastrules │       │  fastrules  │
  │  -json    │       │  -xml     │       │    -db      │
  │           │       │           │       │             │
  │ JsonLoader│       │ XmlLoader │       │  DbRepository│
  │ JsonRepo  │       │ XmlRepo   │       │  (SOCI)     │
  └───────────┘       └───────────┘       └─────────────┘
  nlohmann/json       pugixml             SOCI
```

**Core has zero external persistence dependencies.** Extensions add JSON/XML/DB support only when needed.

## Extension Architecture

Extensions live in `extensions/` and follow a common pattern:

```
extensions/
├── json/
│   ├── include/fastrules/json_loader.hpp    → Workflow/Rule serialization
│   ├── include/fastrules/json_repository.hpp → CRUD via JSON files
│   ├── src/
│   └── tests/
├── xml/
│   ├── include/fastrules/xml_loader.hpp      → XML serialization
│   ├── include/fastrules/xml_repository.hpp  → CRUD via XML files
│   ├── src/
│   └── tests/
└── db/
    ├── include/fastrules/db_repository.hpp   → SOCI persistence
    ├── src/
    └── tests/
```

Each extension:
- Builds as a separate CMake target (`fastrules-json`, `fastrules-xml`, `fastrules-db`)
- Links against `FastRules` core
- Can be built standalone (finds core via `find_package(FastRules)`)
- Exposes namespace `fastrules::ext` for backward compatibility

## Execution Flow

```
1. Create Workflow
   ├── C++ API: Rule objects + workflow.rules.push_back()
   └── JSON API: JsonLoader::loadWorkflow(jsonStr)

2. Validate()
   ├── Check syntax (compile test)
   ├── Check dependency existence
   └── Detect circular deps (DFS)

3. Compile()
   ├── For each rule:
   │   ├── Compile expression ──▶ Lua function
   │   ├── Compile action ──▶ Lua function
   │   └── Recurse childRules
   └── Store refs in Lua registry

4. Execute(parameters)
   ├── Create RuleContext
   ├── resolveExecutionOrder() ──▶ topological sort + priority
   ├── For each rule in order:
   │   ├── Skip if !isActive
   │   ├── Check dependency succeeded
   │   ├── Execute childRules (bottom-up)
   │   ├── Evaluate expression ──▶ Lua call
   │   ├── Execute action ──▶ Lua call
   │   └── Store result in context
   └── Return vector<RuleResult>
```

## Rule Lifecycle

| Phase | Mutable? | Operations |
|-------|----------|------------|
| Definition | Yes | Set properties, expressions, actions |
| Validation | Yes | Syntax check, dependency validation |
| Compilation | No | Compile to Lua, freeze properties |
| Execution | No | Read-only evaluation |

## Memory Safety

- `Rule` is not sealed (C++ doesn't have the concept), but `isCompiled` flag prevents modification
- `RuleContext` uses `shared_mutex` for thread-safe result storage
- `LuaEngine` registry manages Lua reference lifetime
- LuaBridge3 provides type-safe C++ bindings

## Performance Characteristics

| Operation | RoslynRules | FastRules |
|-----------|-------------|-----------|
| First compile | ~50ms (Roslyn) | ~1ms (Lua parse) |
| Subsequent calls | Nanoseconds (IL) | Microseconds (Lua bytecode) |
| Rule evaluation | Near-native | Fast enough for 10K+ rules/sec |
| Memory per rule | Assembly bytes | Lua function reference |
| Parallel execution | True parallel | True parallel via per-thread engine clones |

### Parallel Execution

`Workflow::executeParallel()` groups rules by dependency level and executes rules within each level in parallel using `std::async`. Each worker thread receives its own cloned `LuaEngine` with compiled expressions, enabling true parallel evaluation without Lua state contention.

Key characteristics:
- **Dependency ordering preserved**: Levels execute sequentially (lower levels complete before higher levels start)
- **True intra-level parallelism**: Rules within a level execute simultaneously on separate LuaEngine clones
- **Thread-safe context**: `RuleContext` uses `std::shared_mutex` for concurrent result access
- **Pre-compiled clone pool**: `Workflow::compile()` creates `hardware_concurrency()` cloned engines with all rules pre-compiled, eliminating per-task allocation overhead

For the target use case (business rules that change frequently but execute many times), this provides excellent throughput for independent rule evaluation.

## Extension APIs

### JSON Extension

```cpp
#include <fastrules/json_loader.hpp>

// Load from string
auto workflow = fastrules::JsonLoader::loadWorkflow(jsonStr);
auto rule = fastrules::JsonLoader::loadRule(jsonStr);

// Save to string
std::string json = fastrules::JsonLoader::saveWorkflow(workflow);
std::string json = fastrules::JsonLoader::saveRule(rule);

// Repository (CRUD)
fastrules::ext::JsonRuleRepository repo("rules.json");
repo.save(rule);
repo.flush();
auto found = repo.findById("rule-id");
```

### XML Extension

```cpp
#include <fastrules/xml_loader.hpp>

// Load from string
auto workflow = fastrules::XmlLoader::loadWorkflow(xmlStr);

// Repository (CRUD)
fastrules::ext::XmlRuleRepository repo("rules.xml");
repo.save(rule);
repo.flush();
```

### DB Extension

```cpp
#include <fastrules/db_repository.hpp>

// Requires SOCI session
soci::session sql("sqlite3://rules.db");
fastrules::ext::DbRuleRepository repo(sql);
repo.save(rule);
auto found = repo.findById("rule-id");
```

## Future Enhancements

1. **LuaJIT Support**: Compile-time option for near-native speed
2. **Bytecode Caching**: Save compiled Lua bytecode to disk
3. ~~**Parallel Execution**: `std::async` or thread pool per dependency level~~ ✅ Implemented - pre-compiled engine clone pool
4. **Async/Await**: Coroutine-based async rule evaluation
5. **Per-Rule Cache**: Memoization with TTL
6. **REST API**: HTTP interface for rule management
7. **Web UI**: Visual rule editor
