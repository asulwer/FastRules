---
layout: default
title: Rule
parent: API Reference
nav_order: 5
---

# Rule

```cpp
#include <fastrules/rule.hpp>
```

## Factory

```cpp
static std::shared_ptr<Rule> create(
    const std::string& id,
    const std::string& expression,
    bool isActive = true);
```

## Properties

| Property | Type | Description |
|----------|------|-------------|
| `id` | `std::string` | Unique identifier |
| `expression` | `std::string` | Lua expression to evaluate |
| `action` | `std::string` | Optional Lua action |
| `description` | `std::string` | Human-readable description |
| `isActive` | `bool` | If false, always returns true |
| `priority` | `int` | Execution order within a level (lower = earlier) |
| `dependsOnRuleId` | `std::optional<std::string>` | Parent rule ID |
| `childRules` | `std::vector<std::shared_ptr<Rule>>` | Nested rules |
| `timeout` | `std::optional<std::size_t>` | Not yet implemented |
| `cacheDuration` | `std::optional<std::size_t>` | Not yet implemented |

## Methods

### compile

```cpp
void compile(LuaEngine& engine);
```

Compiles expression and action into Lua functions. Cached on the engine.

### validate

```cpp
void validate(const std::vector<Rule>& allRules);
```

Checks for circular dependencies and missing parent rules.

### execute

```cpp
RuleResult execute(LuaEngine& engine, 
    const std::vector<RuleParameter>& parameters);
```

Executes the rule with given parameters.

**Returns:** `RuleResult`

## RuleResult

```cpp
struct RuleResult {
    std::string ruleId;
    bool success = false;
    std::optional<std::any> value;
    std::optional<std::exception_ptr> exception;

    bool isSuccess() const noexcept { return success; }
};
```

## JSON Schema

```json
{
    "id": "adult-check",
    "description": "Adult customer check",
    "expression": "customer.age >= 18",
    "action": "callbacks.setProcessed(customer, true)",
    "isActive": true,
    "priority": 1,
    "dependsOnRuleId": null,
    "childRules": [],
    "timeout": null,
    "cacheDuration": null
}
```
