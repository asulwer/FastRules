---
layout: default
title: API Reference
nav_order: 2
has_children: true
permalink: /api/
---

# API Reference

Quick reference for core classes. See child pages for full details.

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
