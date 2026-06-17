---
layout: default
title: Examples
nav_order: 4
has_children: false
---

# Examples

Complete examples showcasing FastRules core features: rules, workflows, child rules, type registration, JSON loading, and multiple parameters.

## Quick Complete Example

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
    auto parent = Rule::create(1, "customer.balance >= minBalance")
        .withAction("approved = true")
        .build();

    // Child rules (execute first, bubble-up)
    auto identity = Rule::create(2, "verified == true").build();
    auto income   = Rule::create(3, "income >= minIncome").build();
    parent->childRules = {identity, income};

    // Workflow with dependency: rule 1 only runs if signup passed
    auto signup = Rule::create(4, "string.len(email) > 0 and age >= 13").build();
    signup->dependsOnRuleId = parent->id;

    Workflow workflow;
    workflow.id = 1;
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

---

## Core Library Examples

The `examples/` folder contains standalone programs demonstrating each feature:

### Basic Usage

| File | Description |
|---|---|
| [`simple_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/simple_example.cpp) | Basic rules with primitive parameters |
| [`core_only_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/core_only_example.cpp) | Core library only, no extensions |
| [`typed_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/typed_example.cpp) | C++ struct binding and method calls |

### Advanced Features

| File | Description |
|---|---|
| [`custom_methods_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/custom_methods_example.cpp) | Type registration + action callbacks |
| [`parent_child_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/parent_child_example.cpp) | Child rules and bubble-up execution |
| [`workflow_bubble_up.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/workflow_bubble_up.cpp) | Parent/child with dependency chain |
| [`multi_object_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/multi_object_example.cpp) | Multiple registered types in one workflow |
| [`no_globals_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/no_globals_example.cpp) | Explicit parameter passing (no Lua globals) |
| [`loop_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/loop_example.cpp) | Batch processing with shared Lua state |
| [`macro_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/macro_example.cpp) | Type registration using macros |

### Debugging & Performance

| File | Description |
|---|---|
| [`console_logging_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/console_logging_example.cpp) | Custom logger integration |
| [`debug_context.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/debug_context.cpp) | Debug output and context inspection |
| [`performance_test.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/performance_test.cpp) | Benchmark and performance metrics |

### Backend & Tools

| File | Description |
|---|---|
| [`luabridge3_basic_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/luabridge3_basic_example.cpp) | LuaBridge3 backend minimal example |
| [`repl_example.cpp`](https://github.com/asulwer/FastRules/blob/master/examples/repl_example.cpp) | Interactive Lua expression testing |

---

## JSON Extension Examples

Located in `extensions/json/examples/`:

| File | Description |
|---|---|
| [`example_customer_rules.cpp`](https://github.com/asulwer/FastRules/blob/master/extensions/json/examples/example_customer_rules.cpp) | Load customer validation rules from JSON |
| [`example_customer_validation.cpp`](https://github.com/asulwer/FastRules/blob/master/extensions/json/examples/example_customer_validation.cpp) | Full customer validation workflow |
| [`example_customer_validation_simple.cpp`](https://github.com/asulwer/FastRules/blob/master/extensions/json/examples/example_customer_validation_simple.cpp) | Simplified validation example |
| [`example_dependency_rules.cpp`](https://github.com/asulwer/FastRules/blob/master/extensions/json/examples/example_dependency_rules.cpp) | Rule dependencies in JSON |
| [`example_hierarchical_rules.cpp`](https://github.com/asulwer/FastRules/blob/master/extensions/json/examples/example_hierarchical_rules.cpp) | Parent-child rule hierarchies |

---

## Multi-Language Examples

| Language | Location |
|---|---|
| C# | `examples/csharp_example/` - .NET integration via P/Invoke |
| Python | `examples/python_example/` - Python bindings (ctypes) |

---

## REPL Interactive Testing

Build and run the interactive Lua expression tester:

```bash
./build/examples/repl
> age = 25
> age >= 18
true
> math.sqrt(3^2 + 4^2)
5.0
> string.len("hello")
5
```

---

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
