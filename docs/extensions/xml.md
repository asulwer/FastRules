---
layout: default
title: XML Extension
parent: Extensions
nav_order: 2
---

# XML Extension

The `fastrules-xml` extension lets you load and save rules and workflows as XML. Useful for integrating with legacy systems, enterprise configuration management, or when JSON is not an option.

## Installation

```bash
# vcpkg (recommended)
vcpkg install pugixml

# Or use the version fetched automatically by CMake
```

```cmake
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
target_link_libraries(your_target fastrules fastrules-xml)
```

## XML Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<workflow id="validation" description="Customer validation">
    <rule id="age-check" priority="1">
        <expression>age >= 18</expression>
        <action>eligible = true</action>
    </rule>
    <rule id="name-check" priority="2">
        <expression>string.len(name) > 0</expression>
    </rule>
</workflow>
```

## Loading a Workflow

```cpp
#include <fastrules/xml_loader.hpp>
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

    // Load from XML
    auto xml = readFile("rules.xml");
    auto workflow = fastrules::XmlLoader::loadWorkflow(xml);
    workflow.compile(engine);

    // Execute
    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);

    auto results = workflow.execute(engine, params);
    return 0;
}
```

## Saving a Workflow

```cpp
#include <fastrules/xml_serialization.hpp>

std::string xml = fastrules::XmlLoader::saveWorkflow(workflow);
std::ofstream("rules.xml") << xml;
```

## XML Schema Reference

| Element | Attribute | Required | Description |
|---|---|---|---|
| `workflow` | `id` | Yes | Workflow identifier |
| `workflow` | `description` | No | Description |
| `rule` | `id` | Yes | Rule identifier |
| `rule` | `priority` | No | Execution order |
| `rule` | `active` | No | `"true"` or `"false"` |
| `expression` | — | Yes | Lua expression |
| `action` | — | No | Lua action |
| `childRules` | — | No | Nested rules |
| `dependsOnRuleId` | — | No | Dependency rule ID |

## Error Handling

```cpp
try {
    auto workflow = fastrules::XmlLoader::loadWorkflow(xml);
} catch (const fastrules::XmlParseException& e) {
    std::cerr << "Invalid XML: " << e.what() << "\n";
}
```
