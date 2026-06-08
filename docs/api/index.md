---
layout: default
title: API Reference
nav_order: 3
has_children: true
permalink: /api/
---

# API Reference


## Quick Reference

### LuaEngine

The core engine that compiles and executes Lua expressions.

```cpp
LuaEngine engine;  // Opens a fresh Lua state with sandboxed environment

// Compile
auto ref = engine.compileExpression("value > 10", {"value"});
auto actionRef = engine.compileAction("result = true", {});

// Execute
RuleContext ctx;
bool result = engine.evaluateExpression(ref.value(), params, ctx);
engine.executeAction(actionRef.value(), params, ctx);
```

### Type Registration

```cpp
struct Customer {
    int age;
    std::string name;
};

engine.registerType<Customer>("Customer", [](auto& ut) {
    ut["age"] = &Customer::age;
    ut["name"] = &Customer::name;
});
```

### Enum Registration

```cpp
enum class Status { Active, Inactive };
registerEnum<Status>(engine.state(), "Status", {
    {Status::Active, "Active"},
    {Status::Inactive, "Inactive"}
});
```

### Rule

```cpp
auto rule = std::make_shared<Rule>();
rule->id = "check-age";
rule->expression = "customer.age >= 18";
rule->action = "eligible = true";
rule->parameterNames = {"customer"};
rule->timeout = std::chrono::milliseconds(100);
rule->cacheDuration = std::chrono::milliseconds(5000);
rule->priority = 10;

rule->compile(engine);
auto result = rule->execute(engine, ctx, params);
```

### Workflow

```cpp
Workflow workflow;
workflow.id = "my-workflow";
workflow.rules.push_back(rule1);
workflow.rules.push_back(rule2);

workflow.validate();
auto results = workflow.execute(engine, params);
```

### JSON Loading

```cpp
#include <fastrules/json_loader.hpp>

auto workflow = fastrules::JsonLoader::loadWorkflowFromFile("rules.json");
std::string json = fastrules::JsonLoader::saveWorkflowPretty(workflow);
```

---

*See child pages for detailed API documentation.*
