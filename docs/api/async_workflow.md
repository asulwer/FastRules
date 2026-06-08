---
layout: default
title: AsyncWorkflow
parent: API Reference
nav_order: 7
---

# AsyncWorkflow

```cpp
#include <fastrules/async_workflow.hpp>
```

Parallel and streaming execution variants of `Workflow`.

## executeStreaming

Process results as they arrive:

```cpp
workflow.executeStreaming(engine, params, [](const RuleResult& r) {
    std::cout << r.ruleId << ": " << (r.isSuccess() ? "PASS" : "FAIL") << "\n";
});
```

## executeParallelAsync

Execute rules in parallel, get all results via future:

```cpp
auto future = workflow.executeParallelAsync(engine, params, 4);  // max 4 concurrent
auto results = future.get();
```

## AsyncRegistry

Register workflows for async dispatch:

```cpp
AsyncRegistry registry;
registry.registerWorkflow("validation", std::move(workflow));

auto future = registry.executeAsync("validation", engine, params);
auto results = future.get();
```
