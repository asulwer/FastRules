---
layout: default
title: Advanced Topics
nav_order: 12
has_children: true
permalink: /advanced/
---

# Advanced Topics

- <https://asulwer.github.io/FastRules/advanced/custom-methods.html> — Inject C++ methods and functions into Lua
- <https://asulwer.github.io/FastRules/advanced/aot-and-versioning.html> — Pre-compile workflows, version rules

## Coroutines and Async/Await

FastRules supports Lua coroutines for long-running or multi-step rules.

```cpp
// A coroutine expression that can yield
auto coro = fastrules::Rule::create(
    "async-check",
    "-- coroutine expression\n"
    "local result = fetchData()\n"  -- hypothetical async call
    "coroutine.yield()\n"
    "return result.status == 'ok'",
    true
);
coro->isCoroutine = true;

// Compile as coroutine
engine.compileCoroutine(coro->expression, {});

// Resume until complete
bool done = engine.resumeCoroutine(ref, params, context);
// Or use await
auto result = engine.await(ref, params, context);
```

## Thread-Safe Execution

For parallel execution, use `AsyncWorkflow`:

```cpp
fastrules::AsyncWorkflow async(engine, workflow);
auto results = async.execute(params);
```

Under the hood:
1. Builds dependency levels via topological sort
2. Executes each level in parallel via `std::async`
3. Each thread gets a cloned `LuaEngine` with independent state

## Custom Predicates

The engine comes with built-in predicates:

| Predicate | Lua Usage | Description |
|-----------|-----------|-------------|
| `isNull` | `isNull(x)` | Checks nil or none |
| `isNotEmpty` | `isNotEmpty(str)` | String length > 0 |
| `isEmpty` | `isEmpty(str)` | String length == 0 |
| `countGreaterThan` | `countGreaterThan(tbl, n)` | Table size > n |
| `countLessThan` | `countLessThan(tbl, n)` | Table size < n |
| `contains` | `contains(tbl, val)` | Value in table |

Add custom predicates via `LuaEngine::state()`:

```cpp
engine.state().set_function("isEven", [](int x) { return x % 2 == 0; });
```

## Performance Tips

1. **Compile once, execute many** — Call `workflow.compile(engine)` once, then `execute()` many times
2. **Reuse LuaEngine** — Don't recreate the engine per request
3. **Use `AsyncWorkflow`** — Parallelize independent rules
4. **Avoid large tables** — Table iteration in Lua is slower than primitives
5. **Prefer `int` over `double`** — Integer math is faster in Lua

## Security

- Expressions run in a sandboxed Lua environment
- No access to `os`, `io`, or `debug` libraries
- No access to file system or network
- Actions are explicitly registered C++ callbacks only
