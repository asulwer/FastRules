---
layout: default
title: Migration Guide
nav_order: 16
---

# Migration Guide

Instructions for upgrading between FastRules versions.

## Version 0.1.0 → 0.2.0

### Breaking Changes

#### LuaEngine API Changes

**Old (0.1.0):**
```cpp
LuaEngine engine;
engine.compile("expression");  // Returns int
```

**New (0.2.0):**
```cpp
LuaEngine engine;
engine.compileExpression("expression");  // Returns std::optional<int>
```

**Migration:** Replace `compile()` with `compileExpression()` and handle optional:
```cpp
auto ref = engine.compileExpression("true");
if (ref.has_value()) {
    // Use ref.value()
}
```

#### Rule Factory Changes

**Old (0.1.0):**
```cpp
auto rule = Rule::create("id", "expression", true);
```

**New (0.2.0):**
```cpp
auto rule = Rule::create(1, "expression", true)
    .withAction("action")
    .build();
```

**Migration:** Use builder pattern and numeric IDs:
```cpp
// Update: "id" → 1 (int)
// Update: Add .build()
auto rule = Rule::create(1, "expression", true).build();
```

#### Workflow Execution Changes

**Old (0.1.0):**
```cpp
auto results = workflow.execute(params);
```

**New (0.2.0):**
```cpp
auto results = workflow.execute(engine, params);
```

**Migration:** Pass LuaEngine as first argument:
```cpp
auto results = workflow.execute(engine, params);
```

### New Features in 0.2.0

#### C API Added

You can now use FastRules from C:

```c
#include <fastrules.h>

fastrules_engine_t engine = fastrules_engine_create();
// ... use C API ...
fastrules_engine_destroy(engine);
```

See [C API documentation](c_api.md) for details.

#### AsyncWorkflow Coroutines

Native C++20 coroutine support:

```cpp
AsyncWorkflow async(workflow, 4);
auto task = coExecuteWorkflow(workflow, engine, params, 4);
auto results = co_await task;
```

#### Engine Pool Management

Better control over parallel execution:

```cpp
LuaEngine* engine = pool.acquire();
// ... use engine ...
pool.release(engine);
```

### Deprecations

The following APIs are deprecated and will be removed in 0.3.0:

| Deprecated | Replacement |
|------------|-------------|
| `Rule::dependsOnRuleId` | `Rule::dependsOnRuleName` |
| `LuaEngine::compile()` | `LuaEngine::compileExpression()` |
| `Workflow::execute(params)` | `Workflow::execute(engine, params)` |

## Version 0.0.9 → 0.1.0

### Breaking Changes

#### ExpressionValidator Return Type

**Old:**
```cpp
bool valid = ExpressionValidator::validate(expr);
```

**New:**
```cpp
auto result = ExpressionValidator::validate(expr);
bool valid = result.valid;
```

**Migration:** Use new result struct with errors and warnings:
```cpp
auto result = ExpressionValidator::validate(expr);
if (!result.valid) {
    for (const auto& err : result.errors) {
        std::cerr << err << "\n";
    }
}
```

#### RateLimiter Configuration

**Old:**
```cpp
RateLimiter limiter;
limiter.configure("rule-id", 10, 100);
```

**New:**
```cpp
RateLimiter limiter;
limiter.configure({
    .ruleId = "rule-id",
    .perSecond = 10,
    .perMinute = 100
});
```

**Migration:** Use designated initializers:
```cpp
limiter.configure({
    .ruleId = "my-rule",
    .perSecond = 10,
    .perMinute = 100
});
```

### New Features in 0.1.0

#### Action Callbacks

Register C++ functions as Lua actions:

```cpp
engine.registerAction("sendEmail", [](const std::any& target, 
                                       const std::vector<std::any>& args) {
    auto email = std::any_cast<std::string>(args[0]);
    // Send email
    return true;
});
```

#### Execution Tracing

Debug rule execution:

```cpp
ExecutionTracer tracer(workflow.id);
tracer.start();
// ... execute rules ...
tracer.finish(true);

auto slowest = tracer.getTrace().getSlowestStep();
```

## General Migration Tips

### 1. Enable Compiler Warnings

```cmake
target_compile_options(your_target PRIVATE
    -Wall -Wextra -Wdeprecated-declarations
)
```

### 2. Use Static Analysis

```bash
# Run clang-tidy
cmake -DCMAKE_CXX_CLANG_TIDY="clang-tidy" -B build -S .

# Run cppcheck
cppcheck --enable=all --std=c++23 src/
```

### 3. Test Incrementally

1. Compile with new version
2. Fix deprecation warnings
3. Run unit tests
4. Run integration tests
5. Deploy to staging

### 4. Version Pinning

Pin to specific version in CMake:

```cmake
FetchContent_Declare(
    fastrules
    GIT_REPOSITORY https://github.com/asulwer/fastrules.git
    GIT_TAG v0.2.0  # Pin to version
)
```

## FAQ

### Q: Do I need to migrate immediately?

A: Deprecated APIs remain for at least one minor version. You have time to migrate.

### Q: Will my rules still work?

A: Rule expressions (Lua) are backward compatible. Only C++ API changes.

### Q: Can I mix versions?

A: No. All components must use same FastRules version.

### Q: Where are release notes?

A: See [GitHub Releases](https://github.com/asulwer/FastRules/releases).
