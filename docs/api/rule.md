---
layout: default
title: Rule
parent: API Reference
nav_order: 1
---

# Rule

```cpp
#include <fastrules/rule.hpp>
```

## Construction

### Builder Pattern (Recommended)

```cpp
auto rule = Rule::create("check-age", "age >= 18")
    .withAction("eligible = true")
    .withPriority(10)
    .withTimeout(std::chrono::milliseconds(100))
    .withCacheDuration(std::chrono::milliseconds(5000))
    .active(true)
    .build();
```

### Manual Construction

```cpp
Rule rule;
rule.id = "check-age";
rule.expression = "age >= 18";
rule.action = "eligible = true";
rule.priority = 10;
rule.isActive = true;
```

## Properties

| Property | Type | Description |
|---|---|---|
| `id` | `std::string` | Unique identifier |
| `expression` | `std::string` | Lua expression to evaluate |
| `action` | `std::string` | Optional Lua action executed on success |
| `description` | `std::string` | Human-readable description |
| `isActive` | `bool` | If false, skipped during execution |
| `priority` | `int` | Execution order (lower = earlier) |
| `dependsOnRuleId` | `std::optional<std::string>` | Must succeed before this rule runs |
| `childRules` | `std::vector<std::shared_ptr<Rule>>` | Child rules execute first (bubble-up) |
| `timeout` | `std::optional<std::chrono::milliseconds>` | Per-rule timeout |
| `cacheDuration` | `std::optional<std::chrono::milliseconds>` | Cache successful results |

## Methods

### compile

```cpp
void compile(LuaEngine& engine);
```

Compiles expression and action into Lua functions. One-time setup.

### validate

```cpp
void validate(const std::vector<std::reference_wrapper<const Rule>>& allRules);
```

Checks for circular dependencies and missing dependencies.

### execute

```cpp
RuleResult execute(LuaEngine& engine, RuleContext& context, 
    const std::vector<RuleParameter>& parameters);
```

Executes the rule. Child rules run first. Parent only evaluates if all children pass.

**Returns:** `RuleResult`

## RuleResult

```cpp
struct RuleResult {
    std::string ruleId;
    bool success = false;
    bool skipped = false;
    std::optional<RuleException> exception;
    std::chrono::steady_clock::time_point executedAt;
    std::vector<RuleResult> childResults;

    [[nodiscard]] bool isSuccess() const noexcept;
};
```

## Convenience Factories

```cpp
// Rule::create returns a Builder
Rule::Builder Rule::create(const std::string& id, 
    const std::string& expression, bool active = true);

// Pre-built common rules
static Rule contains(const std::string& param, const std::string& substring);
static Rule equals(const std::string& param, int value);
static Rule range(const std::string& param, int min, int max);
static Rule regex(const std::string& param, const std::string& pattern);
```
