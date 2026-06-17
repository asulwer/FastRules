---
layout: default
title: Lua Version Compatibility
nav_order: 13
---

# Lua Version Compatibility

## Supported Versions

FastRules supports **Lua 5.1 through 5.4** and **LuaJIT** via LuaBridge3.

## LuaBridge3 Backend

FastRules uses **LuaBridge3** as its Lua binding library. LuaBridge3 provides:

- Clean C++ template-based bindings
- Support for custom types, functions, and coroutines
- Header-only (no separate compilation needed)
- Active maintenance and Lua 5.1–5.4 compatibility

### Technical Details

LuaBridge3 wraps the Lua C API to provide type-safe C++ access:

```cpp
// LuaBridge3 usage example
luabridge::LuaRef func = luabridge::getGlobal(L, "myFunction");
auto result = func(arg1, arg2);
```

Unlike sol2's compile-time template machinery, LuaBridge3 uses runtime type checking with clean error messages.

## What This Means for Users

### If using vcpkg

FastWorks uses FetchContent to manage dependencies. CMake fetches Lua 5.4 directly from the official Lua GitHub mirror.

```json
// vcpkg.json
{
  "dependencies": ["lua", "doctest", "nlohmann-json"]
}
```

LuaBridge3 is fetched via CMake's FetchContent.

### If using system packages

Ensure your system Lua is 5.1–5.4:

```bash
# Ubuntu/Debian
apt install liblua5.4-dev   # ✅

# macOS
brew install lua@5.4          # ✅

# Windows with vcpkg
vcpkg install lua            # Use 5.4
```

### If using LuaJIT

LuaJIT is supported and often preferred for production:

```cmake
-DFASTRULES_USE_LUAJIT=ON
```

LuaJIT provides:
- Near-native execution speed via JIT compilation
- Smaller memory footprint
- Deterministic FFI for C++ interop

## Timeline for New Lua Versions

Support for new Lua versions will be added once LuaBridge3 supports them. Track upstream:

- LuaBridge3 GitHub: `https://github.com/kunitoki/LuaBridge3`
- Lua changelog: `https://lua.org/work/doc/`

---

*Last updated: 2026-06-09*
