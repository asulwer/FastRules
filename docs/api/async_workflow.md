---
layout: default
title: AsyncWorkflow
parent: API Reference
nav_order: 2
---

# AsyncWorkflow

```cpp
#include <fastrules/async_workflow.hpp>
```

Provides parallel execution of independent rules using thread-safe LuaEngine clones.

## Construction

```cpp
fastrules::LuaEngine engine;
fastrules::Workflow workflow;
// ... add rules ...

fastrules::AsyncWorkflow async(engine, workflow);
```

## Methods

### execute

```cpp
std::vector<RuleResult> execute(
    const std::vector<RuleParameter>& parameters = {});
```

Executes rules in parallel where possible, respecting dependencies.

- Rules with `dependsOnRuleId` wait for their parent
- Independent rules run concurrently via `std::async`
- Each thread gets a cloned LuaEngine

### getExecutionOrder

```cpp
std::vector<std::vector<std::shared_ptr<Rule>>> getExecutionOrder() const;
```

Returns rules grouped by dependency level (for inspection).

## Example

```cpp
fastrules::LuaEngine engine;

auto rule1 = fastrules::Rule::create("check-a", "a > 10", true);
auto rule2 = fastrules::Rule::create("check-b", "b < 5", true);
auto rule3 = fastrules::Rule::create("check-c", "c == true", true);

fastrules::Workflow workflow;
workflow.rules = {rule1, rule2, rule3};
workflow.compile(engine);

fastrules::AsyncWorkflow async(engine, workflow);
auto results = async.execute({
    {"a", std::any(20)},
    {"b", std::any(3)},
    {"c", std::any(true)}
});

// check-a and check-b run in parallel
// check-c runs in parallel (no dependencies)
```

## Thread Safety

`AsyncWorkflow` creates independent `LuaEngine` clones for each thread via `LuaEngine::clone()`. Each clone has its own Lua state, registry, and type bindings.
