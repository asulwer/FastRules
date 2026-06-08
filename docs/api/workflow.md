---
layout: default
title: Workflow
parent: API Reference
nav_order: 7
---

# Workflow

```cpp
#include <fastrules/workflow.hpp>
```

## Construction

```cpp
fastrules::Workflow workflow;
workflow.description = "Customer validation";
```

## Properties

| Property | Type | Description |
|----------|------|-------------|
| `id` | `std::string` | Unique identifier |
| `description` | `std::string` | Human-readable description |
| `isActive` | `bool` | Master switch |
| `rules` | `std::vector<std::shared_ptr<Rule>>` | Top-level rules |
| `compiledExpressionRefs` | `std::vector<int>` | Internal compiled refs |
| `compiledActionRefs` | `std::vector<std::optional<int>>` | Internal action refs |

## Methods

### compile

```cpp
void compile(LuaEngine& engine);
```

Compiles all rules in the workflow.

### execute

```cpp
std::vector<RuleResult> execute(
    LuaEngine& engine, 
    const std::vector<RuleParameter>& parameters = {});
```

Executes all rules respecting dependencies and priorities.

**Rules with `dependsOnRuleId`** run after their parent.

### executeChildRules

```cpp
std::vector<RuleResult> executeChildRules(
    LuaEngine& engine,
    const std::vector<RuleParameter>& parameters = {});
```

Executes only child rules (useful for nesting).

### validate

```cpp
void validate() const;
```

Checks for circular dependencies across all rules.

### toJson

```cpp
[[nodiscard]] nlohmann::json toJson() const;
```

Serializes to JSON.

### fromJson

```cpp
static std::shared_ptr<Workflow> fromJson(const nlohmann::json& j);
```

Deserializes from JSON.

## JSON Schema

```json
{
    "id": "customer-validation",
    "description": "Customer validation workflow",
    "isActive": true,
    "rules": [
        {
            "id": "adult-check",
            "expression": "customer.age >= 18",
            "action": "callbacks.setProcessed(customer, true)",
            "isActive": true,
            "priority": 1,
            "dependsOnRuleId": null,
            "childRules": [],
            "timeout": null,
            "cacheDuration": null
        }
    ]
}
```
