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

    // 2. Define a rule
    Rule rule;
    rule.id = "age-check";
    rule.expression = "age >= 18";
    rule.action = "eligible = true";
    rule.parameterNames = {"age"};

    // 3. Compile the rule (one-time setup)
    rule.compile(engine);

    // 4. Execute with parameters
    RuleContext ctx;
    std::vector<RuleParameter> params;
    params.emplace_back("age", "int", std::any(25));

    auto result = rule.execute(engine, ctx, params);

    // 5. Check result
    std::cout << "Rule " << result.ruleId 
              << (result.isSuccess() ? " passed" : " failed") 
              << "\n";

    return 0;
}
```

## Workflows

Multiple rules execute in sequence:

```cpp
Workflow workflow;
workflow.id = "signup-validation";

auto emailCheck = std::make_shared<Rule>();
emailCheck->id = "email-valid";
emailCheck->expression = "string.find(email, '@') ~= nil";
emailCheck->parameterNames = {"email"};

auto ageCheck = std::make_shared<Rule>();
ageCheck->id = "age-valid";
ageCheck->expression = "age >= 13";
ageCheck->parameterNames = {"age"};

workflow.rules = {emailCheck, ageCheck};
workflow.compile(engine);

auto results = workflow.execute(engine, params);
```

## Child Rules (Bubble-Up)

Child rules execute first. Parent only runs if all children pass:

```cpp
Rule parent;
parent.id = "credit-check";
parent.expression = "score > 650";

auto child1 = std::make_shared<Rule>();
child1->id = "identity-verified";
child1->expression = "verified == true";

auto child2 = std::make_shared<Rule>();
child2->id = "income-sufficient";
child2->expression = "income >= 50000";

parent.childRules = {child1, child2};
```

## Type Registration

Register C++ structs for use in Lua:

```cpp
struct Point {
    double x;
    double y;
};

engine.registerType<Point>("Point", [](auto& ut) {
    ut["x"] = &Point::x;
    ut["y"] = &Point::y;
});

Rule rule;
rule.expression = "math.sqrt(point.x^2 + point.y^2) < 100";
rule.parameterNames = {"point"};
```

## Next Steps

- [JSON Persistence](JSON_EXTENSION.md)
- [XML Persistence](XML_EXTENSION.md)
- [Database Setup](DB_EXTENSION_SETUP.md)
- [Architecture Overview](architecture.md)
