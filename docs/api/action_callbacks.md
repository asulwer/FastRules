---
layout: default
title: ActionCallbacks
parent: API Reference
nav_order: 5
---

# ActionCallbacks

```cpp
#include <fastrules/action_callback.hpp>
```

Backend-neutral storage for C++ callback handlers. Used by `LuaEngine` and bound to Lua by `LuaBackend` implementations.

## Handler Signature

```cpp
using Handler = std::function<void(const std::any& target, const std::vector<std::any>& args)>;
```

- `target` — the object being acted upon (often empty)
- `args` — arguments passed from Lua

## Usage

Register via `LuaEngine`:

```cpp
engine.registerAction("sendEmail", [](const std::any&, const std::vector<std::any>& args) {
    auto recipient = std::any_cast<std::string>(args[0]);
    auto subject = std::any_cast<std::string>(args[1]);
    std::cout << "Email: " << recipient << " / " << subject << "\n";
});
```

Lua action usage:
```lua
-- In a rule action string:
callbacks.sendEmail("alice@example.com", "Welcome")
```

## Direct Access (Advanced)

```cpp
ActionCallbacks callbacks;

// Register
callbacks.registerHandler("log", [](const std::any&, const std::vector<std::any>& args) {
    auto msg = std::any_cast<std::string>(args[0]);
    std::cout << "[LOG] " << msg << "\n";
});

// Check if registered
if (callbacks.hasHandler("log")) {
    callbacks.execute("log", {}, {std::any(std::string("hello"))});
}

// Iterate handlers (used by backends)
callbacks.forEachHandler([](const std::string& name, const Handler& handler) {
    std::cout << "Handler: " << name << "\n";
});

// Clear all
callbacks.clear();
```

## Methods

| Method | Description |
|---|---|
| `registerHandler(name, handler)` | Add a callback |
| `hasHandler(name)` | Check if registered |
| `execute(name, target, args)` | Execute a callback |
| `forEachHandler(fn)` | Iterate all handlers |
| `getHandlerNames()` | List all handler names |
| `clear()` | Remove all handlers |
| `registerStub(name)` | Add no-op stub (prevents Lua errors) |
