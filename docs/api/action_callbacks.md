---
layout: default
title: ActionCallbacks
parent: API Reference
nav_order: 1
---

# ActionCallbacks

```cpp
#include <fastrules/action_callback.hpp>
```

**Deprecated**: Actions can now mutate C++ objects directly via Lua expressions.
Use `customer.processed = true` instead of `callbacks.setProcessed(customer, true)`.

ActionCallbacks are still available for advanced use cases where you need
C++ logic in your actions, but they are no longer required for basic object mutation.

## When to Use ActionCallbacks

Use callbacks only when you need:
- Complex C++ logic in an action
- Access to external systems (databases, network, etc.)
- Thread-safe operations that require C++ synchronization

## Example (Advanced Use)

```cpp
// Register a callback for complex operations
engine.registerAction("logAudit", [](sol::object target, const auto& args) {
    auto* customer = target.as<Customer*>();
    std::string action = args[0].as<std::string>();
    AuditLogger::log(customer->id, action);
});
```

## JSON Usage

```json
{
    "action": "callbacks.logAudit(customer, 'adult_check_passed')"
}
```

## Migration

**Old (callbacks required):**
```json
{"action": "callbacks.setProcessed(customer, true)"}
```

**New (direct mutation):**
```json
{"action": "customer.processed = true"}
```

Direct mutation is simpler, faster, and requires no C++ registration.
