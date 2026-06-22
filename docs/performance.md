---
layout: default
title: Performance Tuning
nav_order: 10
---

# Performance Tuning

## Benchmarks

Typical performance on a modern desktop (AMD Ryzen 7 / Intel i7):

| Operation | Time |
|-----------|------|
| Compile expression | ~1ms |
| Evaluate simple expression | ~10μs |
| Evaluate with usertype | ~50μs |
| Workflow (10 rules, sequential) | ~500μs |
| Workflow (10 rules, parallel) | ~200μs |

## Optimization Tips

### 1. Use Cache

```cpp
rule->cacheDuration = std::chrono::milliseconds(1000);
// Result cached for 1 second
```

### 2. Pre-compile Rules

```cpp
// Compile once at startup
for (auto& rule : workflow.rules) {
    rule->compile(engine);
}

// Execute many times
for (int i = 0; i < 10000; ++i) {
    workflow.execute(engine, params);
}
```

### 3. Batch Parameter Updates

```cpp
// Instead of creating new params each time:
params[0].value = newValue;
```

### 4. Use LuaJIT

```cmake
# Much faster execution
set(FASTRULES_USE_LUAJIT ON)
```

### 5. Monitor Memory

```cpp
if (engine.getMemoryUsageKB() > 1024) {
    engine.resetState();  // Fresh Lua state
}
```

### 6. Streaming for Large Workflows

```cpp
// Yield results as they complete instead of waiting for all
for (auto& result : workflow.executeStreaming(engine, params)) {
    process(result);
}
```

### 7. Parallel for Independent Rules

```cpp
// Rules with no dependencies execute concurrently
// Workflow::compile() pre-creates a pool of cloned engines for parallel execution
auto results = workflow.executeParallel(engine, params);
```

### 8. Reuse Engines with an EnginePool

Lua states are not thread-safe, so parallel execution needs one engine per
thread. `EnginePool` is a lock-free (Treiber) stack for handing engines out to
workers and returning them, avoiding repeated `clone()` cost. The pool stores
pointers only — it does **not** own the engines, so keep them alive externally.

```cpp
#include <fastrules/engine_pool.hpp>

// Create and own the engines elsewhere
std::vector<std::unique_ptr<LuaEngine>> engines;
for (int i = 0; i < 4; ++i) {
    engines.push_back(std::make_unique<LuaEngine>());
}

EnginePool pool;
for (auto& e : engines) {
    pool.push(e.get());
}

// Worker thread
if (LuaEngine* engine = pool.pop()) {  // pop() returns nullptr if empty
    // ... use engine ...
    pool.push(engine);                 // return it for reuse
}

// Or block up to a deadline waiting for one to free up:
if (LuaEngine* engine = pool.tryPop(std::chrono::milliseconds(50))) {
    // ...
    pool.push(engine);
}
```

`Workflow::compile()` already builds an internal clone pool for
`executeParallel()`, so reach for `EnginePool` only when you manage parallelism
yourself.

### 9. Pool Hot Allocations with MemoryManager

For workloads that execute the same workflow millions of times, the per-call
`RuleContext` and result-vector allocations can dominate. `MemoryManager` is a
process-wide singleton of object pools that recycle those objects.

```cpp
#include <fastrules/memory_pool.hpp>

auto& mm = MemoryManager::getInstance();
mm.preallocate();                       // warm the pools at startup

auto ctx = mm.acquireContext();         // std::unique_ptr<RuleContext>
// ... use ctx ...
mm.releaseContext(std::move(ctx));      // return it to the pool

// Inspect pool occupancy
size_t ctxPool, ctxAlloc, resPool, resAlloc;
mm.getStats(ctxPool, ctxAlloc, resPool, resAlloc);
```

The generic `MemoryPool<T>` / `VectorPool<T>` templates back the manager and can
be used directly for your own hot objects; both are thread-safe and bounded by a
`maxSize` (default 1000).

## Memory Profile

| Component | Size |
|-----------|------|
| Lua state (empty) | ~64KB |
| Lua state (with types) | ~256KB |
| Compiled expression | ~2KB |
| Workflow (10 rules) | ~50KB |
| Engine clone pool (per workflow) | `hardware_concurrency()` × ~320KB |
| Per-execution overhead | ~1KB |

## Profiling

Use execution tracing to find bottlenecks:

```cpp
ExecutionTracer tracer(workflow.id);   // constructor takes the workflow id (int)
auto results = workflow.executeWithTrace(engine, params, tracer);

if (auto slowest = tracer.getTrace().getSlowestStep()) {
    std::cout << "Slowest: " << slowest->ruleName << " took "
              << slowest->duration().count() << "ns\n";
}
```

See the [Observability guide](observability.html) for the full tracing and
performance-counter API.
