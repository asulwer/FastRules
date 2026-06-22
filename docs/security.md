---
layout: default
title: Security Guide
nav_order: 11
---

# Security Guide

FastRules runs user-provided Lua code. This guide explains the security model.

## Sandboxing

The Lua environment is sandboxed by default:

**Removed modules:**
- `os` — No file system or shell access
- `io` — No file I/O
- `debug` — No introspection
- `loadfile`, `dofile` — No arbitrary code loading
- `require` — No module loading

## Pre-validation

Expressions and actions are checked for dangerous patterns before compilation:

```cpp
ExpressionValidator::validate("os.execute('rm -rf /')");
// Returns: valid=false, errors=["Dangerous pattern: os.execute()"]
```

**Blocked patterns:**
- `os.execute`, `os.exit`
- `io.open`, `io.popen`
- `loadfile`, `dofile`, `loadstring`, `load`
- `require`, `module`
- `debug.getregistry`, `debug.getlocal`, `debug.setupvalue`
- `rawset`, `rawget`, `setmetatable`, `getmetatable`
- `collectgarbage`, `newproxy`

## Expression Length Limits

```cpp
engine.setMaxExpressionLength(1000);  // Characters
```

Throws `RuleCompilationException` if exceeded.

## Parameter Type Validation

```cpp
std::vector<RuleParameter> params;
params.emplace_back("age", "not an int");  // Wrong! String passed where int expected

ParameterValidator::validateTypes(params);
// Throws ParameterTypeException
```

## Timeout Enforcement

```cpp
rule->timeout = std::chrono::milliseconds(500);
```

Rules exceeding the timeout fail with `RuleTimeoutException`. The Lua instruction count hook fires every 1000 instructions to catch infinite loops.

## Rate Limiting

Rules can be throttled with a token-bucket `RateLimiter`, keyed by rule **name**.
A rule with no limiter attached falls back to the global
`RateLimiter::global()` instance.

```cpp
#include <fastrules/rate_limiter.hpp>

// Configure a limit for a named rule on the global limiter
RateLimiter::global().configure({
    .ruleName = "check-age",
    .maxExecutionsPerSecond = 100,
    .maxExecutionsPerMinute = 1000,
    .burstSize = 0          // 0 = no burst
});

// Or attach a dedicated limiter to a rule via the builder
auto limiter = std::make_shared<RateLimiter>();
limiter->configure({.ruleName = "check-age", .maxExecutionsPerSecond = 100});

auto rule = Rule::create(1, "age >= 18")
    .withName("check-age")
    .withRateLimiter(limiter)
    .build();
```

When a rule exceeds its limit during execution it fails with a
`RateLimitException` (counted under `totalRulesRateLimited` in the
[performance counters](observability.html)). For ad-hoc checks use the
non-throwing `isAllowed(ruleName)` or inspect
`getCurrentExecutionsPerSecond(ruleName)`.

## Best Practices

1. Always validate rules before deployment
2. Set reasonable timeouts
3. Use expression length limits
4. Enable structured logging for audit trails
5. Review the execution trace for unexpected behavior
