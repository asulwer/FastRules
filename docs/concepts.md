---
layout: default
title: Core Concepts
nav_order: 5
---

# Core Concepts

## Rule

A single rule with a Lua expression and optional action.

```cpp
auto rule = fastrules::Rule::create(
    "adult-check",           // id
    "age >= 18",             // expression (Lua)
    true                     // isActive
);
rule->priority = 1;
rule->action = "print('Adult verified')";
```

## Workflow

A collection of rules with dependency ordering.

```cpp
fastrules::Workflow workflow;
workflow.description = "Customer validation";

auto rule1 = fastrules::Rule::create("check-age", "age >= 18", true);
auto rule2 = fastrules::Rule::create("check-name", "#name > 0", true);

workflow.rules = {rule1, rule2};
workflow.compile(engine);

auto results = workflow.execute(engine, params);
```

## LuaEngine

The core that compiles and executes Lua expressions.

```cpp
fastrules::LuaEngine engine;

// Register custom types
engine.registerType<Customer>("Customer", [](auto& ut) {
    ut["age"] = &Customer::age;
    ut["name"] = &Customer::name;
});

// Register action callbacks
engine.registerAction("setProcessed", [](sol::object target, const auto& args) {
    target.as<Customer*>()->processed = args[0].as<bool>();
});
```

## RuleResult

```cpp
struct RuleResult {
    std::string ruleId;
    bool success = false;
    std::optional<std::any> value;
    std::optional<std::exception_ptr> exception;
};
```

## Parameter Passing

Parameters are passed as name-value pairs:

```cpp
std::vector<fastrules::RuleParameter> params = {
    {"age", std::any(25)},
    {"name", std::any(std::string("Alice"))}
};
```

For typed objects:

```cpp
Customer customer{"Alice", 25};
params.emplace_back("customer", &customer);
```

## Dependencies

Rules can depend on other rules:

```cpp
auto child = fastrules::Rule::create("child", "true", true);
child->dependsOnRuleId = "parent";  // Runs after "parent"
```

## Parallel Execution

```cpp
fastrules::AsyncWorkflow async(engine, workflow);
auto results = async.execute(params);
// Executes independent rules in parallel using thread-safe LuaEngine clones
```
