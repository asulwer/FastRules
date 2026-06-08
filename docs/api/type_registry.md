---
layout: default
title: TypeRegistry
parent: API Reference
nav_order: 4
---

# TypeRegistry

```cpp
#include <fastrules/type_registry.hpp>
```

Backend-neutral storage for C++ type descriptors. Used by `LuaEngine` and consumed by `LuaBackend` implementations.

## TypeField

```cpp
struct TypeField {
    std::string name;
    size_t offset;
    std::string luaType;  // "int", "double", "string", "bool"
};
```

## TypeDescriptor

```cpp
struct TypeDescriptor {
    std::string name;
    std::type_index type;
    std::vector<TypeField> fields;
    size_t size = 0;
};
```

## Usage

`TypeRegistry` is owned by `LuaEngine`. You interact with it indirectly via `engine.registerType()`.

```cpp
struct Point { double x, y; };

engine.registerType<Point>("Point", {
    {"x", offsetof(Point, x), "double"},
    {"y", offsetof(Point, y), "double"}
});
```

## Direct Access (Advanced)

```cpp
TypeRegistry registry;

// Register a type
registry.registerType<Point>("Point", {
    {"x", offsetof(Point, x), "double"},
    {"y", offsetof(Point, y), "double"}
});

// Check if registered
if (registry.isRegistered(std::type_index(typeid(Point)))) {
    auto desc = registry.getDescriptor(std::type_index(typeid(Point)));
    std::cout << "Type: " << desc->name << " has " << desc->fields.size() << " fields\n";
}

// Iterate all registered types
for (const auto& [typeIndex, descriptor] : registry.allTypes()) {
    std::cout << descriptor.name << "\n";
}
```

## Methods

| Method | Description |
|---|---|
| `registerType<T>(name, fields)` | Register a C++ struct |
| `isRegistered(std::type_index)` | Check by C++ type |
| `isRegistered(std::string)` | Check by name |
| `getDescriptor(std::type_index)` | Get descriptor by type |
| `getDescriptor(std::string)` | Get descriptor by name |
| `allTypes()` | Iterate all registered types |
