---
layout: default
title: API Reference
nav_order: 100
has_children: true
permalink: /api/
---

# API Reference

Quick reference for core classes. See child pages for full details:

- [Rule](rule.html) — the condition/action pair and its builder
- [Workflow](workflow.html) — orchestrating ordered rule execution
- [LuaEngine](lua_engine.html) — compiling and executing Lua
- [Type Registry](type_registry.html) — binding C++ structs/enums into Lua
- [Action Callbacks](action_callbacks.html) — calling C++ from Lua actions
- [Async Workflow](async_workflow.html) — coroutine-based execution
- [JSON Loader](json_loader.html) — loading rules from JSON (extension)

See also the [Predicate Reference](../predicates.html) and
[Observability guide](../observability.html).

## C API

The C API provides FFI bindings for Python, C#, and other languages. It is always exported from the core library; the header is `include/fastrules/fastrules.h`.

```c
// C example
fastrules_engine_t engine = fastrules_engine_create();
fastrules_workflow_t workflow = fastrules_workflow_create(engine, 1, "validation");
fastrules_workflow_add_rule(engine, workflow, 1, "check-age", "age >= 18", NULL, NULL, true);
fastrules_workflow_compile(engine, workflow);

char* results;
fastrules_workflow_execute(engine, workflow, "age=25", &results);
fastrules_free(results);

fastrules_workflow_destroy(workflow);
fastrules_engine_destroy(engine);
```

See [C API documentation](c_api.html) for full details.

## LuaEngine

Compiles and executes Lua expressions through the LuaBridge3 backend (abstracted behind `LuaBackend`).

```cpp
LuaEngine engine;

// Compile
auto ref = engine.compileExpression("age >= 18");

// Execute with parameters
std::vector<RuleParameter> params;
params.emplace_back("age", 25);
bool pass = engine.evaluateExpression(ref.value(), params, context);
```

## Rule

Rule IDs are integers (`Rule::Id` is `int`). Use `withName(...)` for a
human-readable name (names are what dependencies reference).

```cpp
auto rule = Rule::create(1, "age >= 18")     // id is an int
    .withName("check-age")
    .withAction("eligible = true")
    .withPriority(10)
    .withTimeout(std::chrono::milliseconds(100))
    .build();
```

## Workflow

```cpp
Workflow workflow;
workflow.id = 1;                          // id is an int
workflow.description = "validation";
workflow.rules.push_back(rule1);
workflow.rules.push_back(rule2);

workflow.validate();
workflow.compile(engine);
auto results = workflow.execute(engine, params);
```

## RuleParameter

```cpp
// Primitive — type inferred automatically
params.emplace_back("age", 25);
params.emplace_back("name", std::string("Alice"));
params.emplace_back("verified", true);

// Registered C++ type
params.emplace_back("customer", Customer{"Alice", 30});
```

## Type Registration

```cpp
struct Customer { int age; std::string name; };

engine.registerType<Customer>("Customer", {
    {"age",  offsetof(Customer, age),  "int"},
    {"name", offsetof(Customer, name), "string"}
});
```

## Action Callbacks

```cpp
engine.registerAction("sendEmail", [](const std::any& target, const std::vector<std::any>& args) {
    auto email = std::any_cast<std::string>(args[0]);
    std::cout << "Sending email to: " << email << "\n";
});
```

## JSON Extension

```cpp
#include <fastrules/json_loader.hpp>

auto json = readFile("rules.json");
auto workflow = fastrules::JsonLoader::loadWorkflow(json);
```
