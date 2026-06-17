---
layout: default
title: Troubleshooting
nav_order: 15
---

# Troubleshooting

Common issues and solutions when working with FastRules.

## Build Issues

### CMake can't find Lua

**Error:**
```
Could not find Lua (missing LUA_INCLUDE_DIR, LUA_LIBRARY)
```

**Solutions:**

**Ubuntu/Debian:**
```bash
sudo apt-get install liblua5.4-dev
```

**macOS:**
```bash
brew install lua
```

**Windows (vcpkg):**
```bash
vcpkg install lua
```

### Linker errors for sol2

**Error:**
```
undefined reference to `sol::state::state()`
```

**Cause:** sol2 is header-only but requires Lua library to be linked.

**Solution:** Ensure Lua is linked after FastRules:
```cmake
target_link_libraries(your_target fastrules lua)
```

### Missing extension headers

**Error:**
```
'fastrules/json_loader.hpp' file not found
```

**Cause:** Extensions not enabled in CMake.

**Solution:**
```bash
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
```

## Runtime Issues

### Rule execution always fails

**Symptoms:** All rules return `isSuccess() == false`

**Check:**
1. Is the rule compiled? Call `workflow.compile(engine)` before executing
2. Is the Lua expression valid? Try simple expressions first: `"true"`, `"1 == 1"`
3. Are parameters bound? Check parameter names match expression variables

**Debug:**
```cpp
auto results = workflow.execute(engine, params);
for (const auto& r : results) {
    std::cout << "Rule " << r.ruleId << ": "
              << (r.isSuccess() ? "PASS" : "FAIL") << "\n";
    if (!r.isSuccess() && r.exception.has_value()) {
        std::cout << "  Error: " << r.exception->what() << "\n";
    }
}
```

### Lua expression syntax errors

**Error:**
```
RuleCompilationException: [lua] expression syntax error
```

**Common mistakes:**
- Using `=` instead of `==` for comparison: ❌ `age = 18` ✅ `age == 18`
- Missing `then` in conditionals: Lua doesn't allow `if x then` in expressions
- Using `null` instead of `nil`: Lua uses `nil`, not `null`
- String concatenation with `+`: Use `..` instead: `"Hello " .. name`

### Timeout exceptions

**Error:**
```
RuleTimeoutException: Rule execution exceeded timeout
```

**Causes:**
1. Infinite loops in Lua expressions
2. Very large table operations
3. Complex regex matching on large strings

**Solutions:**
- Add timeout to rule: `rule.timeout = std::chrono::milliseconds(100)`
- Check for infinite loops: Avoid `while true do ... end` without break
- Optimize expressions: Pre-calculate values in C++, pass as parameters

### Rate limiting triggered unexpectedly

**Symptoms:** Rules fail with "Rate limit exceeded" on first execution

**Check:**
1. Rate limiter configuration:
```cpp
RateLimiter::global().configure({
    .ruleId = "my-rule",
    .perSecond = 10,  // Allow 10 per second
    .perMinute = 100  // Allow 100 per minute
});
```

2. Is the rule ID unique? Duplicate IDs share rate limits.

## Performance Issues

### Slow rule execution

**Diagnosis:**
```cpp
// Enable performance counters
auto& counters = PerformanceCounters::instance();
// ... execute rules ...
auto stats = counters.getCounters();
std::cout << "Avg time: " << stats.averageExecutionTimeNs << " ns\n";
```

**Common causes:**
1. **Recompiling on every request:** Compile once, execute many
2. **Large table iteration:** Avoid `for k,v in pairs(huge_table)` in rules
3. **String concatenation in loops:** Build strings in C++ instead
4. **No caching:** Enable rule caching with `rule.cacheDuration`

**Solutions:**
```cpp
// Good: Compile once
workflow.compile(engine);
for (auto request : requests) {
    auto results = workflow.execute(engine, request.params);  // Fast
}

// Bad: Compile every time
for (auto request : requests) {
    workflow.compile(engine);  // Slow!
    auto results = workflow.execute(engine, request.params);
}
```

### Memory leaks in long-running apps

**Symptoms:** Memory grows over time

**Causes:**
1. Not releasing compiled expression references
2. Growing Lua registry
3. Circular references in rule results

**Solutions:**
```cpp
// Periodically cleanup
engine.cleanupCompiledExpressions();

// Or use RAII
{
    LuaEngine engine;
    // ... use engine ...
} // Engine destroyed, memory freed

// For persistent engines, periodically reset
engine.resetState();
```

## Extension Issues

### JSON loader fails to parse

**Error:**
```
JsonLoaderException: Failed to parse JSON
```

**Check JSON format:**
```json
{
  "id": 1,
  "description": "My workflow",
  "rules": [
    {
      "id": 1,
      "name": "age-check",
      "expression": "age >= 18",
      "active": true
    }
  ]
}
```

**Common mistakes:**
- Using single quotes: JSON requires double quotes
- Trailing commas: ❌ `"age": 25,` before `}`
- Missing required fields: `id` and `expression` are required

### Database connection fails

**Error:**
```
DbConnectionException: Could not connect to database
```

**Check:**
1. SOCI is properly installed: `sudo apt-get install libsoci-dev`
2. Connection string format: `"postgresql://user:pass@host/db"`
3. Database server is running

## Thread Safety Issues

### Crash in parallel execution

**Symptoms:** Segfault or data corruption with `executeParallel()`

**Cause:** Lua state is not thread-safe. Each thread needs its own LuaEngine.

**Solution:**
```cpp
// Correct: Workflow uses engine pool internally
auto results = workflow.executeParallel(engine, params);

// Correct: AsyncWorkflow for manual control
AsyncWorkflow async(workflow, 4);  // 4 threads
async.compile(engine);
auto results = async.executeParallelAsync(engine, params);

// WRONG: Don't share engines across threads
std::thread t1([&]{ workflow.execute(engine, params); });  // ❌
std::thread t2([&]{ workflow.execute(engine, params); });  // ❌
```

### Race conditions in rule results

**Symptoms:** Intermittent test failures, inconsistent results

**Cause:** Modifying shared data from multiple rules.

**Solution:** Rules should be pure functions. Use RuleContext for passing data:
```cpp
// Good: Each rule returns result, no side effects
RuleResult result = rule.execute(engine, context, params);
bool passed = result.isSuccess();

// Bad: Rules modify shared global
rule.expression = "global_counter = global_counter + 1";  // ❌ Race condition
```

## Debugging Tips

### Enable verbose logging

```cpp
auto logger = spdlog::stdout_color_mt("fastrules");
logger->set_level(spdlog::level::debug);
engine.setLogger(logger);
```

### Check rule context

```cpp
RuleContext ctx;
auto results = workflow.execute(engine, params, ctx);

auto error = ctx.getLastError();
if (error.has_value()) {
    std::cout << "Last error in rule " << error->first
              << ": " << error->second << "\n";
}
```

### Validate before compiling

```cpp
auto validation = ExpressionValidator::validate("my expression");
if (!validation.valid) {
    for (const auto& err : validation.errors) {
        std::cerr << "Validation error: " << err << "\n";
    }
}
```

## Still Stuck?

1. **Check the examples:** `examples/` folder has working code
2. **Read the tests:** `tests/` shows usage patterns
3. **Enable debug build:** `cmake -DCMAKE_BUILD_TYPE=Debug`
4. **File an issue:** https://github.com/asulwer/FastRules/issues
