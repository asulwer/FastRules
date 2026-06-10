---
layout: default
title: LuaEngine
parent: API Reference
nav_order: 3
---

# LuaEngine

```cpp
#include <fastrules/lua_engine.hpp>
```

## Construction

```cpp
fastrules::LuaEngine engine;
```

Opens a fresh Lua state with sandboxed environment (base, string, table, math, coroutine libs).

## Type Registration

Register C++ structs for field access in Lua expressions.

```cpp
template<typename T>
void registerType(const std::string& name, std::vector<TypeField> fields);
```

**Example:**

```cpp
struct Customer {
    std::string name;
    int age;
    double balance;
};

engine.registerType<Customer>("Customer", {
    {"name",     offsetof(Customer, name),     "string"},
    {"age",      offsetof(Customer, age),      "int"},
    {"balance",  offsetof(Customer, balance),  "double"}
});

// In Lua: customer.age >= 18 and customer.balance >= 1000
```

**Fields:**
- `name` — field name exposed to Lua
- `offset` — byte offset from struct base (`offsetof`)
- `luaType` — primitive type tag ("int", "double", "string", "bool")

## Action Registration

Register C++ callbacks for Lua actions.

```cpp
void registerAction(const std::string& name, ActionCallbacks::Handler handler);
```

**Example:**

```cpp
engine.registerAction("sendEmail", [](const std::any& target, const std::vector<std::any>& args) {
    auto email = std::any_cast<std::string>(args[0]);
    std::cout << "Emailing: " << email << "\n";
});

// In Lua action: callbacks.sendEmail("alice@example.com")
```

## Compilation

```cpp
std::optional<int> compileExpression(const std::string& expression);
std::optional<int> compileAction(const std::string& action);
std::optional<int> compileCoroutine(const std::string& expression);
```

Returns a registry reference (`int`) on success, `std::nullopt` on failure.

## Execution

```cpp
bool evaluateExpression(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context,
    std::optional<std::chrono::milliseconds> timeout = std::nullopt);

void executeAction(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context,
    std::optional<std::chrono::milliseconds> timeout = std::nullopt);
```

## Coroutines

```cpp
bool resumeCoroutine(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context);

std::optional<std::any> await(int ref, 
    const std::vector<RuleParameter>& parameters, 
    RuleContext& context);

bool isCoroutine(int ref) const;
```

## Globals

```cpp
void setGlobal(const std::string& name, const std::any& value);
void clearGlobals();
```

## Engine Management

```cpp
void resetState();              // Reset Lua state, keep compiled refs
void collectGarbage();          // Force Lua GC
size_t getMemoryUsageKB();      // Current Lua memory

void setAutoResetThreshold(size_t kb);  // Auto-reset when memory exceeds
size_t getAutoResetThreshold() const;
size_t getCompileCount() const;         // Number of compilations
size_t getGeneration() const;           // State generation (increments on reset)
```

## Backend Access

```cpp
// Raw lua_State (backend-neutral, works with any Lua backend)
lua_State* luaState() const noexcept;
```

`luaState()` returns the raw `lua_State*` pointer for interop with C Lua libraries and tooling.

_If migrating from an older version that used `sol::state&` — that accessor was removed. Use `luaState()` instead._
