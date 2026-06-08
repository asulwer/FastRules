---
layout: default
title: JsonLoader
parent: API Reference
nav_order: 3
---

# JsonLoader

```cpp
#include <fastrules/json_loader.hpp>
```

## Functions

### parseRule

```cpp
static std::shared_ptr<Rule> parseRule(const nlohmann::json& j);
```

Parses a single rule from JSON.

### parseWorkflow

```cpp
static std::shared_ptr<Workflow> parseWorkflow(const nlohmann::json& j);
```

Parses a workflow with all its rules.

### toJson

```cpp
static nlohmann::json toJson(const Workflow& workflow);
static nlohmann::json toJson(const Rule& rule);
```

Serializes to JSON.

## JSON Format

### Rule

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

### Workflow

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
        },
        {
            "id": "name-check",
            "expression": "isNotEmpty(customer.name)",
            "isActive": true,
            "priority": 2,
            "dependsOnRuleId": null,
            "childRules": [],
            "timeout": null,
            "cacheDuration": null
        }
    ]
}
```

## Example

```cpp
#include <fastrules/json_loader.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

// Load from file
std::ifstream file("rules.json");
nlohmann::json j;
file >> j;

auto workflow = fastrules::JsonLoader::parseWorkflow(j);

// Serialize
auto json = fastrules::JsonLoader::toJson(*workflow);
std::cout << json.dump(2) << std::endl;
```
