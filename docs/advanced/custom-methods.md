---
layout: default
title: Injecting C++ Methods
parent: Advanced Topics
nav_order: 2
---

# Injecting C++ Methods into Lua

FastRules provides two mechanisms for injecting user-defined C++ code into Lua expressions and actions. This is the equivalent of RoslynRules' ability to inject C# methods.

---

## Mechanism 1: Type Registration (`registerType<T>`)

Bind C++ struct/ class fields and methods to Lua so they can be used directly in expressions and actions.

### What You Can Do

| From Lua | C++ Required |
|----------|-------------|
| `customer.name` | Field binding |
| `customer.age >= 18` | Field in expression |
| `customer:isPremium()` | Method binding (no args, returns bool) |
| `customer:getTier()` | Method binding (returns string) |
| `customer:addBalance(100)` | Method binding (args + void return) |
| `customer.processed = true` | Field mutation in action |

### C++ Struct Requirements

Your struct needs comparison operators for sol2's automagic support:

```cpp
struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;
    double balance = 0.0;

    // Required by sol2 automagic
    bool operator==(const Customer& other) const { /* ... */ }
    bool operator<=(const Customer& other) const { /* ... */ }
    bool operator<(const Customer& other) const { /* ... */ }

    // Your methods
    bool isPremium() const { return balance > 1000.0; }
    void addBalance(double amount) { balance += amount; }
    std::string getTier() const;
};
```

### Registration

```cpp
fastrules::LuaEngine engine;

engine.registerType<Customer>("Customer", [](auto& ut) {
    // Fields — read/write in both expressions and actions
    ut["name"]      = &Customer::name;
    ut["age"]       = &Customer::age;
    ut["processed"] = &Customer::processed;
    ut["balance"]   = &Customer::balance;

    // Methods — callable from Lua
    ut["isPremium"]  = &Customer::isPremium;
    ut["getTier"]    = &Customer::getTier;
    ut["addBalance"] = &Customer::addBalance;
});
```

### Usage in Rules

```cpp
// Expression using method
rule->expression = "customer:isPremium()";

// Expression using field
rule->expression = "customer.age >= 18 and customer.balance > 0";

// Action mutating field and calling method
rule->action = R"(
    customer.processed = true
    customer:addBalance(10.0)
)";
```

### Passing to Workflow

```cpp
Customer customer{"Alice", 25, false, true, 100.0};

std::vector<fastrules::RuleParameter> params;
// Pass pointer — Lua can mutate the original C++ object
params.emplace_back("customer", "Customer", std::any(&customer));

auto results = workflow.execute(engine, params);
// customer.processed may now be true (if action ran)
```

### Syntax Notes

- **Field access**: `customer.name` (always works)
- **Method call (no args)**: `customer:isPremium()` — use colon `:` syntax
- **Method call (args)**: `customer:addBalance(10.0)` — colon + args
- **Assignment**: `customer.processed = true` — direct mutation

---

## Mechanism 2: Action Callbacks (`registerAction`)

Bind standalone C++ functions to a Lua `callbacks` table for use in actions.

### What You Can Do

| From Lua | C++ Required |
|----------|-------------|
| `callbacks.sendEmail("to", "subj", "body")` | `registerAction("sendEmail", ...)` |
| `callbacks.logAction("rule-id", name, true)` | `registerAction("logAction", ...)` |

### C++ Function

```cpp
void sendEmail(const std::string& to, const std::string& subject,
               const std::string& body) {
    // Your implementation
}
```

### Registration

```cpp
engine.registerAction("sendEmail",
    [](sol::object target, const std::vector<sol::object>& args) {
        // args[0] = to, args[1] = subject, args[2] = body
        if (args.size() >= 3) {
            sendEmail(
                args[0].as<std::string>(),
                args[1].as<std::string>(),
                args[2].as<std::string>()
            );
        }
    }
);
```

### Usage in Actions

```cpp
rule->action = R"(
    customer.processed = true
    callbacks.sendEmail("admin@corp.com", "Alert",
                        "Customer " .. customer.name .. " processed")
)";
```

### Callback Signature

```cpp
void callback(sol::object target, const std::vector<sol::object>& args);
```

- `target`: First argument from Lua (often the object the action operates on)
- `args`: All remaining arguments as a vector
- Return values: Not supported directly; use TypeRegistry methods for value-returning functions

---

## Comparison: Type Registration vs Action Callbacks

| | Type Registration | Action Callbacks |
|---|---|---|
| **What** | C++ struct fields/methods | Standalone C++ functions |
| **Lua syntax** | `customer:method()` or `customer.field` | `callbacks.functionName(...)` |
| **Can mutate** | Yes — direct field/method access | Yes — via arguments |
| **Can return values** | Yes — method return values work | No — use TypeRegistry methods instead |
| **Best for** | Domain objects (Customer, Order, etc.) | Cross-cutting concerns (logging, email, etc.) |
| **Lifespan** | Tied to LuaEngine (re-bind on reset) | Tied to LuaEngine (re-bind on reset) |

---

## Full Working Example

See: [`examples/custom_methods_example.cpp`](https://github.com/asulwer/fastrules/blob/main/examples/custom_methods_example.cpp)

This example demonstrates:
- Customer struct with fields and methods
- Type registration for field access and method calls
- Action callbacks for sendEmail, logAction, formatCurrency
- Rules combining both mechanisms
- Execution showing mutation and callback output

Build and run:
```bash
cmake --build build --target custom_methods_example
./build/examples/custom_methods_example
```

---

## RoslynRules Comparison

| RoslynRules (C#) | FastRules (C++) |
|---|---|
| `customer.Processed = true` | `customer.processed = true` |
| `customer.IsPremium()` | `customer:isPremium()` |
| `SendEmail(to, subject, body)` | `callbacks.sendEmail(to, subject, body)` |
| `customer.AddBalance(100)` | `customer:addBalance(100)` |
| Compile-time type checking | Runtime via sol2 usertype |
| ~50ms compile | ~1ms compile |

The key difference: RoslynRules compiles to IL delegates with direct method calls. FastRules uses Lua as the expression language, so C++ methods are bridged through sol2's usertype system. The methods become Lua userdata methods that call back into C++.

---

## Common Pitfalls

### 1. Missing Comparison Operators

If your struct lacks `==`, `<=`, `<`, sol2's automagic won't work:

```
[error] lua: error: [string "..."]:1: attempt to compare two userdata values
```

**Fix**: Add the three comparison operators.

### 2. Wrong Method Call Syntax

```lua
-- WRONG: customer.isPremium()  -- uses dot, not colon
-- RIGHT: customer:isPremium() -- colon passes self as first arg
```

**Fix**: Use `:` for methods, `.` for fields.

### 3. Pass by Value Instead of Pointer

```cpp
// WRONG: Lua gets a copy, mutations don't affect original
params.emplace_back("customer", "Customer", std::any(customer));

// RIGHT: Lua gets pointer, mutations affect original
params.emplace_back("customer", "Customer", std::any(&customer));
```

**Fix**: Pass `&customer` (pointer) when you want mutations.

### 4. Callback Not Found

```
[error] lua: attempt to call a nil value (global 'callbacks')
```

**Fix**: Register callbacks before compiling rules that use them:
```cpp
engine.registerAction("sendEmail", ...);  // First
workflow.compile(engine);                    // Then compile
```

### 5. Type Not Registered

```
[error] lua: attempt to index a nil value (global 'customer')
```

**Fix**: Register type before compiling rules:
```cpp
engine.registerType<Customer>("Customer", ...);  // First
workflow.compile(engine);                          // Then compile
```
