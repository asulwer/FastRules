---
layout: default
title: Getting Started
nav_order: 1
has_children: false
---

# Getting Started with FastRules

## Installation

### Requirements

| Tool | Minimum Version | Notes |
|---|---|---|
| CMake | 3.28+ | FetchContent for dependencies |
| C++ Compiler | C++23 | Visual Studio 2022, GCC 13+, Clang 17+ |
| Git | 2.30+ | For submodule-like FetchContent |

### Quick Start (Default Build)

```bash
git clone https://github.com/asulwer/fastrules.git
cd fastrules
cmake -B build -S .
cmake --build build --config Release
ctest --output-on-failure
```

Core library (`fastrules`) builds with zero manual dependency installation. CMake FetchContent downloads sol2 and nlohmann/json automatically.

### Platform-Specific

**Windows (Visual Studio 2022):**
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

**Linux:**
```bash
sudo apt install cmake g++ git
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

**macOS:**
```bash
xcode-select --install
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(sysctl -n hw.ncpu)
```

### Package Managers

**vcpkg:**
```bash
vcpkg install fastrules
```

**Conan:**
```bash
conan install fastrules/0.1.0
```

**CMake FetchContent:**
```cmake
include(FetchContent)
FetchContent_Declare(
    fastrules
    GIT_REPOSITORY https://github.com/asulwer/fastrules.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(fastrules)
target_link_libraries(your_app PRIVATE fastrules)
```

### Build Options

| Option | Default | Description |
|---|---|---|
| `FASTRULES_BUILD_TESTS` | `ON` | Catch2 test suite |
| `FASTRULES_BUILD_EXAMPLES` | `ON` | Example programs |
| `FASTRULES_BUILD_EXTENSIONS` | `OFF` | JSON, XML, DB extensions |
| `FASTRULES_BUILD_DB` | `OFF` | Database extension (requires SOCI) |
| `FASTRULES_LUA_BACKEND` | `sol2` | `sol2` or `luabridge3` |

---

## Your First Rule

```cpp
#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

int main() {
    LuaEngine engine;

    auto rule = Rule::create("age-check", "age >= 18")
        .withAction("eligible = true")
        .build();

    Workflow workflow;
    workflow.id = "signup-validation";
    workflow.rules.push_back(rule);
    workflow.compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);

    auto results = workflow.execute(engine, params);

    std::cout << "Rule " << results[0].ruleId
              << (results[0].isSuccess() ? " passed" : " failed") << "\n";

    return 0;
}
```

---

## Multiple Parameters

Pass any number of parameters. All become Lua globals before execution:

```cpp
std::vector<RuleParameter> params;
params.emplace_back("age", 25);                          // int
params.emplace_back("name", std::string("Alice"));      // string
params.emplace_back("verified", true);                  // bool
params.emplace_back("score", 720.5);                    // double
params.emplace_back("customer", Customer{"Alice", 30}); // registered type

// In Lua expressions:
//   age >= 18 and verified == true
//   customer.age >= 18 and score > 650
//   string.len(name) > 0

auto results = workflow.execute(engine, params);
```

---

## Workflows

Multiple rules execute in sequence, respecting priority and dependencies:

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

---

## Child Rules (Bubble-Up)

Child rules execute first. Parent only evaluates if all children pass:

```cpp
auto parent = Rule::create("credit-check", "customer.balance >= minBalance")
    .withAction("approved = true")
    .build();

auto identity = Rule::create("identity-verified", "verified == true").build();
auto income = Rule::create("income-sufficient", "income >= minIncome").build();

parent->childRules = {identity, income};

Workflow workflow;
workflow.rules.push_back(parent);
workflow.compile(engine);

std::vector<RuleParameter> params;
params.emplace_back("customer", Customer{"Alice", 30, 5000.0});
params.emplace_back("verified", true);
params.emplace_back("income", 75000.0);
params.emplace_back("minBalance", 1000.0);
params.emplace_back("minIncome", 40000.0);

auto results = workflow.execute(engine, params);
```

---

## Type Registration

Register C++ structs for field access in Lua:

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

---

## Next Steps

- [Architecture Overview](architecture.md)
- [Core Concepts](concepts.md)
- [Examples](examples.md)
- [Extensions](extensions/)
