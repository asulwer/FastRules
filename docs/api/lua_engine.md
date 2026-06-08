---
layout: default
title: LuaEngine
parent: API Reference
nav_order: 4
---

# LuaEngine

```cpp
#include <fastrules/lua_engine.hpp>
```

## Construction

```cpp
fastrules::LuaEngine engine;
```

Opens Lua base libs: base, string, table, math, coroutine, package.

## Type Registration

Register C++ structs for use in Lua expressions.

```cpp
template<typename T>
void registerType(const std::string& name, typename TypeBinder<T>::BinderFunc binder);
```

**Example:**

```cpp
struct Customer {
    std::string name;
    int age;
    bool processed;
};

engine.registerType<Customer>("Customer", [](auto& ut) {
    ut["name"] = &Customer::name;
    ut["age"] = &Customer::age;
    ut["processed"] = &Customer::processed;
});
```

Now in expressions: `customer.age >= 18`

## Action Registration

Register C++ callbacks for Lua actions.

```cpp
void registerAction(const std::string& name, 
    std::function<void(sol::object target, const std::vector<sol::object>& args)> handler);
```

**Example:**

```cpp
engine.registerAction("setProcessed", [](sol::object target, const auto& args) {
    if (target.is<Customer*>() && !args.empty()) {
        target.as<Customer*>()->processed = args[0].as<bool>();
    }
});
```

Lua usage: `callbacks.setProcessed(customer, true)`

## Compilation

```cpp
std::optional<int> compileExpression(
    const std::string& expression, 
    const std::vector<std::string>& parameterNames = {});

std::optional<int> compileAction(
    const std::string& action,
    const std::vector<std::string>& parameterNames = {});

std::optional<int> compileCoroutine(
    const std::string& expression,
    const std::vector<std::string>& parameterNames = {});
```

Returns a registry reference (int) or nullopt on failure.

## Execution

```cpp
bool evaluateExpression(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context);

void executeAction(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context);
```

## Coroutines

```cpp
bool resumeCoroutine(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context);

std::optional<sol::object> await(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context);

bool isCoroutine(int ref) const;
```

## Globals

```cpp
void setGlobal(const std::string& name, const std::any& value);
void clearGlobals();
```

## Clone

```cpp
std::unique_ptr<LuaEngine> clone() const;
```

Creates a thread-safe copy with independent Lua state.

## Raw Access

```cpp
sol::state& state() noexcept;
```

Access the underlying sol2 state. Use with care.
