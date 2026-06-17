---
layout: default
title: API Reference
nav_order: 4
has_children: true
permalink: /api/
---

# API Reference

Quick reference for core classes. See child pages for full details.

## C API

The C API provides FFI bindings for Python, C#, and other languages. Built into the shared library when `FASTRULES_BUILD_SHARED` and `FASTRULES_BUILD_C_API` are enabled.

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

Compiles and executes Lua expressions. Backend-agnostic (sol2 or LuaBridge3).

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

```cpp
auto rule = Rule::create("check-age", "age >= 18")
    .withAction("eligible = true")
    .withPriority(10)
    .withTimeout(std::chrono::milliseconds(100))
    .build();
```

## Workflow

```cpp
Workflow workflow;
workflow.id = "validation";
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
