---
layout: default
title: Examples
nav_order: 7
---

# Examples

## Table of Contents

- <https://asulwer.github.io/fastrules/examples/simple.html> — Basic rules with primitives
- <https://asulwer.github.io/fastrules/examples/json.html> — Loading rules from JSON
- <https://asulwer.github.io/fastrules/examples/typed.html> — C++ structs in Lua expressions
- <https://asulwer.github.io/fastrules/examples/custom-methods.html> — Inject C++ methods and callbacks into Lua
- <https://asulwer.github.io/fastrules/examples/customer.html> — Full business logic with callbacks
- <https://asulwer.github.io/fastrules/examples/repl.html> — Interactive Lua expression testing

## Simple Example

Basic rules with primitive parameters (int, string, bool).

```cpp
#include <fastrules.hpp>
#include <iostream>

int main() {
    fastrules::LuaEngine engine;

    auto rule1 = fastrules::Rule::create("adult-check", "age >= 18", true);
    auto rule2 = fastrules::Rule::create("name-check", "#name > 0", true);

    fastrules::Workflow workflow;
    workflow.rules = {rule1, rule2};
    workflow.compile(engine);

    std::vector<fastrules::RuleParameter> params = {
        {"age", std::any(25)},
        {"name", std::any(std::string("Alice"))}
    };

    auto results = workflow.execute(engine, params);
    // results[0].success == true (age 25 >= 18)
    // results[1].success == true (name not empty)

    return 0;
}
```

## JSON Workflow

Load rules from a JSON file instead of inline C++.

```json
{
    "id": "customer-validation",
    "rules": [
        {
            "id": "adult-check",
            "expression": "customer_age >= 18"
        },
        {
            "id": "name-check",
            "expression": "#customer_name > 0"
        }
    ]
}
```

```cpp
#include <fastrules/json_loader.hpp>
#include <fstream>
#include <sstream>

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    auto jsonText = readFile("rules.json");
    auto workflow = fastrules::JsonLoader::parseWorkflow(
        nlohmann::json::parse(jsonText));

    workflow->compile(engine);
    auto results = workflow->execute(engine, params);
}
```

## Typed Parameters

Pass C++ structs into Lua expressions.

```cpp
struct Customer {
    std::string name;
    int age;
    bool operator==(const Customer& o) const { 
        return name == o.name && age == o.age; 
    }
    bool operator<=(const Customer& o) const { return age <= o.age; }
    bool operator<(const Customer& o) const { return age < o.age; }
};

engine.registerType<Customer>("Customer", [](auto& ut) {
    ut["name"] = &Customer::name;
    ut["age"] = &Customer::age;
});

Customer customer{"Alice", 25};
std::vector<fastrules::RuleParameter> params;
params.emplace_back("customer", "Customer", std::any(&customer));

// In Lua: customer.age >= 18  -- works!
auto rule = fastrules::Rule::create("check", "customer.age >= 18", true);
```

## Full Customer Validation

Complete example with type registration, action callbacks, and JSON loading.

See: [examples/typed_example.cpp](../examples/typed_example.cpp)

Key features:
- `engine.registerType()` for C++ struct binding
- `engine.registerAction()` for C++ callbacks
- `RuleParameter` with type annotation
- Action mutation of C++ objects

## Custom C++ Methods Example

Comprehensive example showing both type registration and action callbacks together.

See: [examples/custom_methods_example.cpp](../examples/custom_methods_example.cpp)

This example demonstrates:
- Customer struct with fields (`name`, `age`, `balance`) and methods (`isPremium()`, `getTier()`, `addBalance()`)
- Type registration for field access and method calls
- Action callbacks for `sendEmail`, `logAction`, `formatCurrency`
- Rules combining both mechanisms
- Execution showing mutation and callback output

For full documentation: [Advanced Topics — Custom C++ Methods](advanced/custom-methods.md)

Key features:
- `engine.registerType<T>()` binds struct fields and methods
- `engine.registerAction()` binds standalone functions
- `customer:isPremium()` — method call in expression
- `customer.processed = true` — field mutation in action
- `callbacks.sendEmail(...)` — standalone function call in action
