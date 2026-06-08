---
layout: default
title: Performance Tuning
nav_order: 6
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
auto results = workflow.executeParallel(engine, params);
```

## Memory Profile

| Component | Size |
|-----------|------|
| Lua state (empty) | ~64KB |
| Lua state (with types) | ~256KB |
| Compiled expression | ~2KB |
| Workflow (10 rules) | ~50KB |
| Per-execution overhead | ~1KB |

## Profiling

Use execution tracing to find bottlenecks:

```cpp
ExecutionTracer tracer("profile");
auto results = workflow.executeWithTrace(engine, params, tracer);

auto slowest = tracer.getTrace().getSlowestStep();
std::cout << "Slowest: " << slowest->ruleId << " took "
          <> slowest->duration().count() << "ns\n";
```
