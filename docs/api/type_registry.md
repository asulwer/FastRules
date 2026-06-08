---
layout: default
title: TypeRegistry
parent: API Reference
nav_order: 6
---

# TypeRegistry

```cpp
#include <fastrules/type_registry.hpp>
```

Maps C++ types to Lua usertypes via sol2.

## LuaEngine.registerType

```cpp
template<typename T>
void LuaEngine::registerType(
    const std::string& name, 
    typename TypeBinder<T>::BinderFunc binder);
```

**Parameters:**
- `name` — Lua type name (e.g., "Customer")
- `binder` — Lambda that binds properties: `[](sol::usertype<T>& ut) { ... }`

## Example

```cpp
struct Customer {
    std::string name;
    int age;
    bool processed = false;
};

engine.registerType<Customer>("Customer", [](auto& ut) {
    ut["name"] = &Customer::name;
    ut["age"] = &Customer::age;
    ut["processed"] = &Customer::processed;
});

// Expressions access properties directly
auto rule = fastrules::Rule::create("check", "customer.age >= 18", true);
rule->action = "customer.processed = true";  // Direct mutation!

Customer customer{"Alice", 25};
std::vector<fastrules::RuleParameter> params;
params.emplace_back("customer", "Customer", std::any(&customer));

auto result = rule->execute(engine, params);
// customer.processed == true (if age >= 18)
```

## Notes

- Types no longer need comparison operators (==,<=,<) - automagic is disabled
- Actions can mutate C++ objects directly via Lua expressions
- No callback registration needed for object mutation
- Pointers are passed as light userdata: `std::any(&customer)`
- The type name in `RuleParameter` must match the registered name
- Types are bound to each LuaEngine clone automatically
