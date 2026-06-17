---
layout: default
title: Parallel Execution
nav_order: 8
---

# Parallel Execution Architecture

FastRules provides two distinct APIs for parallel rule execution. Understanding when to use each is important for correct and efficient code.

## Overview

| Feature | `Workflow::executeParallel` | `AsyncWorkflow` |
|---------|------------------------------|-----------------|
| **Use Case** | One-shot parallel execution | Long-running, repeated async operations |
| **Thread Pool** | Created per-execution | Persistent across executions |
| **Engine Clones** | Created per-execution | Persistent, pre-compiled |
| **API Style** | Synchronous (blocking) | Asynchronous (non-blocking) |
| **Best For** | Simple parallel workflows | High-throughput, concurrent workloads |
| **Overhead** | Higher (setup each time) | Lower (amortized over many calls) |

## When to Use `executeParallel`

Use `Workflow::executeParallel` when:

- You need to execute a workflow **once** with parallel rules
- You want **synchronous** blocking behavior (caller waits for results)
- You're already using `execute()` and just want parallel execution for this run
- You don't need to execute the same workflow multiple times

```cpp
Workflow workflow = /* ... */;
LuaEngine engine;

// Compile once
workflow.compile(engine);

// Execute in parallel - blocks until complete
auto results = workflow.executeParallel(engine, params);
// Results are ready here
```

**How it works:**
1. Creates a thread pool sized to hardware concurrency
2. Clones the Lua engine for each thread
3. Compiles rules into each clone
4. Executes rules in parallel across dependency levels
5. Blocks until all rules complete
6. Returns results (thread pool destroyed)

## When to Use `AsyncWorkflow`

Use `AsyncWorkflow` when:

- You need to execute workflows **repeatedly**
- You want **asynchronous** non-blocking behavior
- You need to integrate with async code (coroutines, futures, callbacks)
- You want to amortize setup costs over many executions

```cpp
// Create once - expensive setup happens here
AsyncWorkflow async(std::move(workflow));
async.compile(engine);  // Pre-compiles into persistent engine pool

// Execute many times - cheap
auto future1 = async.executeAsync(engine, params1);
auto future2 = async.executeAsync(engine, params2);
auto results = async.executeParallelAsync(engine, params3);  // Non-blocking

// Wait for completion when needed
async.waitForCompletion();
```

**How it works:**
1. Creates a **persistent** thread pool on construction
2. Pre-compiles rules into a pool of engine clones
3. Provides async APIs that return futures/promises
4. Reuses thread pool and engine clones across executions
5. Requires explicit `waitForCompletion()` or future.get()

## Detailed Comparison

### Setup Cost

```cpp
// executeParallel: Setup every time
for (int i = 0; i < 1000; i++) {
    workflow.executeParallel(engine, params);  // Creates thread pool each time
}

// AsyncWorkflow: Setup once
AsyncWorkflow async(std::move(workflow));
async.compile(engine);  // Setup once here
for (int i = 0; i < 1000; i++) {
    async.executeParallelAsync(engine, params);  // Reuses thread pool
}
```

### Engine Clone Management

**executeParallel:**
- Creates fresh clones on each execution
- No risk of stale state between runs
- Higher per-execution overhead

**AsyncWorkflow:**
- Reuses engine clones from pool
- Faster execution (no clone creation)
- Clones are reset between uses

### Thread Safety

Both APIs are thread-safe:

- `executeParallel`: Safe to call from multiple threads (each call creates isolated resources)
- `AsyncWorkflow`: Safe to call from multiple threads (internal synchronization)

However:
- Don't modify the Workflow while `executeParallel` is running
- Don't modify the AsyncWorkflow while async operations are pending

## Integration with C++20 Coroutines

`AsyncWorkflow` provides native coroutine support:

```cpp
// Coroutine-based execution
Task<void> processRules(AsyncWorkflow& async) {
    auto results = co_await async.coExecuteAsync(engine, params);
    // Process results
}
```

`executeParallel` is synchronous and doesn't integrate with coroutines.

## Error Handling

### executeParallel

Exceptions propagate synchronously:

```cpp
try {
    auto results = workflow.executeParallel(engine, params);
} catch (const RuleException& e) {
    // Handle error
}
```

### AsyncWorkflow

Exceptions are captured in futures:

```cpp
auto future = async.executeAsync(engine, params);
try {
    auto results = future.get();
} catch (const RuleException& e) {
    // Handle error
}
```

Or in the async result wrapper:

```cpp
auto results = async.executeParallelAsync(engine, params);
for (const auto& result : results) {
    if (!result.isSuccess()) {
        // Handle error from specific rule
    }
}
```

## Performance Guidelines

| Scenario | Recommendation | Reason |
|----------|---------------|--------|
| Single execution | `executeParallel` | Simpler API, no persistent resources |
| High-frequency (100s/sec) | `AsyncWorkflow` | Amortized setup cost |
| Long-running service | `AsyncWorkflow` | Persistent resources, better resource management |
| One-shot batch job | `executeParallel` | No cleanup needed |
| Mixed sync/async code | `AsyncWorkflow` | Consistent async interface |

## Common Mistakes

### ❌ Using executeParallel in a hot loop

```cpp
// Bad: Creates/destroys thread pool every iteration
for (const auto& request : requests) {
    workflow.executeParallel(engine, request.params);  // Expensive!
}
```

### ✅ Using AsyncWorkflow for repeated execution

```cpp
// Good: Amortize setup cost
AsyncWorkflow async(std::move(workflow));
async.compile(engine);
for (const auto& request : requests) {
    async.executeParallelAsync(engine, request.params);
}
async.waitForCompletion();
```

### ❌ Forgetting to call waitForCompletion

```cpp
// Bad: Async operations may not complete before destruction
AsyncWorkflow async(std::move(workflow));
async.compile(engine);
async.executeAsync(engine, params);
// async destroyed here - operations may be cancelled!
```

### ✅ Proper cleanup

```cpp
// Good: Wait for completion before destruction
AsyncWorkflow async(std::move(workflow));
async.compile(engine);
auto future = async.executeAsync(engine, params);
future.wait();  // or async.waitForCompletion();
```

## Migration Guide

If you're using `executeParallel` and want to migrate to `AsyncWorkflow`:

1. **Change construction:**
   ```cpp
   // Before
   Workflow workflow = /* ... */;
   
   // After
   AsyncWorkflow async(std::move(workflow));
   ```

2. **Change compilation:**
   ```cpp
   // Before
   workflow.compile(engine);
   
   // After
   async.compile(engine);  // Compiles into persistent pool
   ```

3. **Change execution:**
   ```cpp
   // Before (blocking)
   auto results = workflow.executeParallel(engine, params);
   
   // After (async)
   auto results = async.executeParallelAsync(engine, params);
   async.waitForCompletion();
   ```

4. **Consider using futures:**
   ```cpp
   // For true async
   auto future = async.executeAsync(engine, params);
   // ... do other work ...
   auto results = future.get();
   ```

## Summary

- Use **`executeParallel`** for simple, one-off parallel execution
- Use **`AsyncWorkflow`** for high-throughput, repeated, or async workloads
- **Never** use `executeParallel` in a tight loop - use `AsyncWorkflow` instead
- **Always** wait for async completion before destroying `AsyncWorkflow`
