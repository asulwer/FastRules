---
layout: default
title: Getting Started
nav_order: 1
has_children: false
---

# Getting Started with FastRules

## Installation

### Prerequisites

- CMake 3.28+
- C++23 compiler (GCC 13+, Clang 17+, MSVC 2022+)
- Git

### Quick Install

**Via vcpkg:**
```bash
vcpkg install fastrules
```

**Via Conan:**
```bash
conan install fastrules/0.1.0
```

**From source:**
```bash
git clone https://github.com/asulwer/fastrules.git
cd fastrules
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON
cmake --build build --config Release
cmake --install build --prefix /usr/local
```

## Your First Rule

```cpp
#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

int main() {
    // 1. Create a Lua engine
    LuaEngine engine;

    // 2. Define a rule using the Builder
    auto rule = Rule::create("age-check", "age >= 18")
        .withAction("eligible = true")
        .build();

    // 3. Create a workflow and add the rule
    Workflow workflow;
    workflow.id = "signup-validation";
    workflow.rules.push_back(rule);

    // 4. Compile the workflow (one-time setup)
    workflow.compile(engine);

    // 5. Execute with parameters
    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);

    auto results = workflow.execute(engine, params);

    // 6. Check result
    std::cout << "Rule " << results[0].ruleId 
              << (results[0].isSuccess() ? " passed" : " failed") 
              << "\n";

    return 0;
}
```

## Workflows

Multiple rules execute in sequence:

```cpp
Workflow workflow;
workflow.id = "signup-validation";

auto emailCheck = Rule::create("email-valid", "string.find(email, '@') ~= nil").build();
auto ageCheck = Rule::create("age-valid", "age >= 13").build();

workflow.rules = {emailCheck, ageCheck};
workflow.compile(engine);

std::vector<RuleParameter> params;
params.emplace_back("email", std::string("user@example.com"));
params.emplace_back("age", 25);

auto results = workflow.execute(engine, params);
```

## Child Rules (Bubble-Up)

Child rules execute first. Parent only runs if all children pass:

```cpp
auto parent = Rule::create("credit-check", "score > 650").build();

auto child1 = Rule::create("identity-verified", "verified == true").build();
auto child2 = Rule::create("income-sufficient", "income >= 50000").build();

parent->childRules = {child1, child2};

Workflow workflow;
workflow.rules.push_back(parent);
workflow.compile(engine);
```

## Type Registration

Register C++ structs for use in Lua:

```cpp
struct Point {
    double x;
    double y;
};

engine.registerType<Point>("Point", {
    {"x", offsetof(Point, x), "double"},
    {"y", offsetof(Point, y), "double"}
});

Rule rule;
rule.id = "distance-check";
rule.expression = "math.sqrt(point.x^2 + point.y^2) < 100";
```

## Next Steps

- [JSON Persistence](json_extension.md)
- [XML Persistence](xml_extension.md)
- [Database Setup](db_extension_setup.md)
- [Architecture Overview](architecture.md)
