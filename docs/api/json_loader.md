---
layout: default
title: JsonLoader
parent: API Reference
nav_order: 6
---

# JsonLoader

```cpp
#include <fastrules/json_loader.hpp>
```

Requires linking `fastrules-json`.

## Loading

```cpp
// From string
std::string json = readFile("rules.json");
auto workflow = fastrules::JsonLoader::loadWorkflow(json);

// From nlohmann::json
nlohmann::json j = ...;
auto workflow = fastrules::JsonLoader::loadWorkflow(j);
```

## Saving

```cpp
// To string
std::string json = fastrules::JsonLoader::saveWorkflow(workflow);

// To file
std::ofstream("rules.json") << json;
```

## JSON Format

```json
{
  "id": "validation",
  "description": "Customer validation workflow",
  "rules": [
    {
      "id": "age-check",
      "expression": "age >= 18",
      "action": "eligible = true",
      "priority": 1,
      "active": true
    },
    {
      "id": "name-check",
      "expression": "string.len(name) > 0",
      "priority": 2
    }
  ]
}
```

## Schema Reference

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | int | Yes | Workflow identifier |
| `description` | string | No | Description |
| `rules` | array | Yes | List of rules |
| `rules[].id` | int | Yes | Rule identifier |
| `rules[].expression` | string | Yes | Lua expression |
| `rules[].action` | string | No | Lua action |
| `rules[].priority` | int | No | Order (lower first) |
| `rules[].active` | bool | No | Enabled (default true) |
| `rules[].dependsOnRuleId` | int | No | Dependency rule ID |
| `rules[].childRules` | array | No | Nested child rules |
| `rules[].timeoutMs` | int | No | Timeout in ms |
| `rules[].cacheDurationMs` | int | No | Cache duration in ms |
