---
layout: default
title: Examples
nav_order: 5
has_children: false
---

# Examples

One example showcasing FastRules core features: rules, workflows, child rules, type registration, and multiple parameters.

## Complete Example

```cpp
#include <fastrules.hpp>
#include <iostream>
#include <iomanip>

using namespace fastrules;

struct Customer {
    std::string name;
    int age;
    double balance;
};

int main() {
    LuaEngine engine;

    // Register C++ type for Lua access
    engine.registerType<Customer>("Customer", {
        {"name", offsetof(Customer, name), "string"},
        {"age",  offsetof(Customer, age),  "int"},
        {"balance", offsetof(Customer, balance), "double"}
    });

    // Parent rule: credit check (only runs if children pass)
    auto parent = Rule::create("credit-check", "customer.balance >= minBalance")
        .withAction("approved = true")
        .build();

    // Child rules (execute first, bubble-up)
    auto identity = Rule::create("identity-verified", "verified == true").build();
    auto income   = Rule::create("income-sufficient", "income >= minIncome").build();
    parent->childRules = {identity, income};

    // Workflow with dependency: credit-check only runs if signup passed
    auto signup = Rule::create("signup-valid", "string.len(email) > 0 and age >= 13").build();
    signup->dependsOnRuleId = parent->id;  // credit-check must pass first

    Workflow workflow;
    workflow.id = "customer-onboarding";
    workflow.description = "Full customer validation pipeline";
    workflow.rules = {parent, signup};
    workflow.compile(engine);

    // Execute with MULTIPLE parameters
    std::vector<RuleParameter> params;
    params.emplace_back("customer", Customer{"Alice", 30, 5000.0});
    params.emplace_back("email",    std::string("alice@example.com"));
    params.emplace_back("age",      30);
    params.emplace_back("verified", true);
    params.emplace_back("income",   75000.0);
    params.emplace_back("minBalance", 1000.0);   // config parameter
    params.emplace_back("minIncome",  40000.0);   // config parameter

    auto results = workflow.execute(engine, params);

    std::cout << "=== Results ===\n";
    for (const auto& r : results) {
        std::cout << std::setw(25) << r.ruleId << "  "
                  << (r.isSuccess() ? "PASS" : "FAIL") << "\n";
    }

    return 0;
}
```

## What This Shows

| Feature | Code |
|---|---|
| **Multiple parameters** | `params.emplace_back("age", 30); params.emplace_back("customer", Customer{...});` |
| **Type registration** | `engine.registerType<Customer>("Customer", {...})` |
| **Child rules** | `parent->childRules = {identity, income}` |
| **Dependencies** | `signup->dependsOnRuleId = parent->id` |
| **Config values** | `minBalance` and `minIncome` as regular parameters |
| **Actions** | `"approved = true"` runs on Lua side |

## Detailed Examples by Feature

The `examples/` folder in the repo has standalone programs for each feature:

| File | Feature |
|---|---|
| `simple_example.cpp` | Basic rules with primitive parameters |
| `core_only_example.cpp` | Core library only, no extensions |
| `typed_example.cpp` | C++ struct binding and method calls |
| `custom_methods_example.cpp` | Type registration + action callbacks |
| `parent_child_example.cpp` | Child rules and bubble-up execution |
| `workflow_bubble_up.cpp` | Parent/child with dependency chain |
| `multi_object_example.cpp` | Multiple registered types in one workflow |
| `no_globals_example.cpp` | Explicit parameter passing (no Lua globals) |
| `loop_example.cpp` | Batch processing with shared Lua state |
| `console_logging_example.cpp` | Custom logger integration |
| `luabridge3_basic_example.cpp` | LuaBridge3 backend (minimal) |
| `comprehensive_example.cpp` | All features combined |

## JSON/Extension Examples

| File | Extension |
|---|---|
| `extensions/json/examples/json_example.cpp` | Load workflow from JSON |
| `extensions/xml/examples/xml_example.cpp` | Load workflow from XML |
| `extensions/db/examples/db_example.cpp` | Load/save workflow from database |

## REPL

Interactive Lua expression testing:

```bash
./build/examples/repl
> age = 25
> age >= 18
true
> math.sqrt(3^2 + 4^2)
5.0
```

## Multi-Parameter Usage

Any number of parameters can be passed. The engine binds them all as Lua globals before execution:

```cpp
std::vector<RuleParameter> params;
params.emplace_back("userId",   42);                          // int
params.emplace_back("name",     std::string("Alice"));       // string
params.emplace_back("verified", true);                      // bool
params.emplace_back("score",    720.5);                     // double
params.emplace_back("customer", Customer{"Alice", 30});     // registered type

// In Lua expressions:
//   userId > 0 and verified == true
//   customer.age >= 18 and score > 650
//   string.len(name) > 0
```

All parameters are available to all rules in the workflow. Rules reference only the ones they need.
