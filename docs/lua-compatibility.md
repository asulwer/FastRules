---
layout: default
title: Lua Version Compatibility
nav_order: 16
---

# Lua Version Compatibility

## Supported Versions

FastRules supports **Lua 5.1 through 5.4** and **LuaJIT**.

## Why Not Lua 5.5?

Lua 5.5 is **not supported** because our Lua binding library, **sol2**, does not yet support it.

sol2 uses compile-time version detection to match Lua ABI compatibility:

```cpp
// sol2 compatibility check
#if !defined(SOL_LUA_VERSION) || (SOL_LUA_VERSION < 501 || SOL_LUA_VERSION > 504)
    #error "unsupported Lua version (i.e. not Lua 5.1, 5.2, 5.3, or 5.4)"
#endif
```

### Technical Reason

sol2's C++ template machinery is tightly coupled to Lua's C API. Each Lua minor version introduces breaking changes in:
- `lua_resume()` signature (coroutine API)
- `lua_load()` behavior
- Thread/extra space layout (`lua_getextraspace()`)
- Internal stack manipulation macros

Adding Lua 5.5 support requires upstream changes in sol2, not FastRules.

## What This Means for Users

### If using vcpkg

The vcpkg registry currently defaults to Lua 5.5. FastRules pins Lua to **5.4** via our manifest to avoid this incompatibility.

```json
// vcpkg.json — Lua 5.4 is fetched via FetchContent, not vcpkg
{
  "dependencies": ["lua", "catch2", "nlohmann-json", "sol2"]
}
```

Our CMake fetches Lua 5.4 directly from the official Lua GitHub mirror, bypassing vcpkg's 5.5 package.

### If using system packages

Ensure your system Lua is 5.1–5.4:

```bash
# Ubuntu/Debian
apt install liblua5.4-dev   # ✅
apt install liblua5.5-dev   # ❌ not supported

# macOS
brew install lua@5.4        # ✅

# Windows with vcpkg
vcpkg install lua            # Currently 5.5 — use FetchContent instead
```

### If using LuaJIT

LuaJIT is supported and often preferred for production:

```cmake
-DFASTRULES_USE_LUAJIT=ON
```

LuaJIT provides:
- Near-native execution speed via JIT compilation
- Smaller memory footprint
- Deterministic `FFI` for C++ interop

## Timeline for Lua 5.5

Support will be added once **sol2** releases a compatible version. Track upstream:

- sol2 GitHub: `https://github.com/ThePhD/sol2`
- Lua 5.5 changelog: `https://lua.org/work/doc/`

No FastRules code changes are needed — only updating the sol2 dependency version.

---

*Last updated: 2026-06-07*
