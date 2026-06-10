---
layout: default
title: JSON Extension
parent: Extensions
nav_order: 1
---

# JSON Extension

The `fastrules-json` extension lets you load and save rules and workflows as JSON. This is the easiest way to externalize rule definitions so non-C++ developers (product managers, analysts) can author and maintain them.

## Installation

```bash
# vcpkg (recommended)
vcpkg install nlohmann-json

# Or use the version fetched automatically by CMake
```

```cmake
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
target_link_libraries(your_target fastrules fastrules-json)
```

## JSON Format

```json
{
  "id": 1,
  "description": "Customer validation workflow",
  "rules": [
    {
      "id": 1,
      "expression": "age >= 18",
      "action": "eligible = true",
      "priority": 1
    },
    {
      "id": 2,
      "expression": "string.len(name) > 0",
      "priority": 2
    }
  ]
}
```

## Loading a Workflow

```cpp
#include <fastrules/json_loader.hpp>
#include <fastrules.hpp>
#include <fstream>
#include <sstream>

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

int main() {
    LuaEngine engine;

    // Load from JSON
    auto json = readFile("customer_validation.json");
    auto workflow = fastrules::JsonLoader::loadWorkflow(json);
    workflow.compile(engine);

    // Execute
    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);
    params.emplace_back("name", std::string("Alice"));

    auto results = workflow.execute(engine, params);

    for (const auto& r : results) {
        std::cout << r.ruleId << ": "
                  << (r.isSuccess() ? "PASS" : "FAIL") << "\n";
    }

    return 0;
}
```

## Saving a Workflow

```cpp
#include <fastrules/json_serialization.hpp>

// After building a workflow in C++...
Workflow workflow;
// ...add rules...

// Save to JSON
std::string json = fastrules::JsonLoader::saveWorkflow(workflow);
std::ofstream("rules.json") << json;
```

## JSON Schema Reference

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | int | Yes | Workflow identifier |
| `description` | string | No | Human-readable description |
| `rules` | array | Yes | List of rules |
| `rules[].id` | int | Yes | Rule identifier |
| `rules[].expression` | string | Yes | Lua expression |
| `rules[].action` | string | No | Lua action executed on pass |
| `rules[].priority` | int | No | Execution order (lower first) |
| `rules[].active` | bool | No | Enabled/disabled (default true) |
| `rules[].childRules` | array | No | Nested child rules |
| `rules[].dependsOnRuleId` | int | No | Depends on another rule |
| `rules[].timeoutMs` | int | No | Per-rule timeout |
| `rules[].cacheDurationMs` | int | No | Cache duration |

## Error Handling

```cpp
try {
    auto workflow = fastrules::JsonLoader::loadWorkflow(json);
} catch (const fastrules::JsonParseException& e) {
    std::cerr << "Invalid JSON: " << e.what() << "\n";
} catch (const fastrules::RuleValidationException& e) {
    std::cerr << "Invalid rule: " << e.what() << "\n";
}
```
