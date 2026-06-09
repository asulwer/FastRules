---
layout: default
title: Workflow
parent: API Reference
nav_order: 2
---

# Workflow

```cpp
#include <fastrules/workflow.hpp>
```

## Construction

```cpp
Workflow workflow;
workflow.id = 1;
workflow.description = "Full customer validation pipeline";
```

## Properties

| Property | Type | Description |
|---|---|---|
| `id` | `int` | Unique identifier |
| `description` | `std::string` | Human-readable description |
| `rules` | `std::vector<std::shared_ptr<Rule>>` | Top-level rules |
| `isActive` | `bool` | Master switch (default true) |

## Methods

### validate

```cpp
void validate();
```

Checks for duplicate IDs, missing dependencies, circular dependencies. Throws `RuleValidationException` on failure.

### compile

```cpp
void compile(LuaEngine& engine);
```

Compiles all rules. Calls `validate()` first if not already validated.

### execute

```cpp
std::vector<RuleResult> execute(
    LuaEngine& engine, 
    const std::vector<RuleParameter>& parameters = {});
```

Executes rules in priority order, respecting dependencies.

Rules with `dependsOnRuleId` only run if the dependency succeeded.

**Returns:** Vector of results for active rules (skipped rules omitted).

### executeStreaming

```cpp
void executeStreaming(
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters,
    std::function<void(const RuleResult&)> onResult);
```

Executes rules and calls `onResult` for each result as it completes.

### executeParallelAsync

```cpp
std::future<std::vector<RuleResult>> executeParallelAsync(
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters,
    std::size_t maxConcurrent = std::thread::hardware_concurrency());
```

Executes rules in parallel up to `maxConcurrent` threads.

## Builder

```cpp
static Workflow::Builder Workflow::create(int id);
```

```cpp
auto workflow = Workflow::create(1)
    .withDescription("Customer validation")
    .withRule(Rule::create(1, "age >= 18").build())
    .withRule(Rule::create(2, "#email > 0").build())
    .build();
```

## JSON/XML Persistence

Workflows are loaded/saved via extensions, not core:

```cpp
// JSON — requires fastrules-json
auto workflow = fastrules::JsonLoader::loadWorkflow(jsonString);

// XML — requires fastrules-xml  
auto workflow = fastrules::XmlLoader::loadWorkflow(xmlString);

// DB — requires fastrules-db
auto workflow = repo.loadWorkflow("workflow-id");
```

Core `Workflow` has no `fromJson()` or `toJson()` methods.
