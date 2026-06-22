---
layout: default
title: Predicate Reference
nav_order: 5
---

# Predicate Reference

FastRules offers predicates at two levels:

1. **Lua built-in predicates** — functions registered into every Lua state that
   you call from inside an expression or action.
2. **`Rule::` factory methods** — C++ helpers that build a complete `Rule`
   wrapping a common predicate, so you don't have to hand-write the Lua.

## Lua built-in predicates

These are available in any expression or action string. They are
null/short-arg tolerant (a missing argument yields `false` rather than an
error).

| Predicate | Signature | True when |
|-----------|-----------|-----------|
| `isNotNull` | `isNotNull(x)` | `x` is not `nil` |
| `isNull` | `isNull(x)` | `x` is `nil` (or absent) |
| `isEmpty` | `isEmpty(x)` | `x` stringifies to an empty string |
| `isNotEmpty` | `isNotEmpty(x)` | `x` stringifies to a non-empty string |
| `inRange` | `inRange(v, min, max)` | `min <= v <= max` |
| `matchesRegex` | `matchesRegex(s, pat)` | `s` contains the substring `pat` |
| `startsWith` | `startsWith(s, prefix)` | `s` begins with `prefix` |
| `endsWith` | `endsWith(s, suffix)` | `s` ends with `suffix` |
| `hasLength` | `hasLength(s, n)` | `s` has exactly `n` characters |
| `hasMinLength` | `hasMinLength(s, n)` | `s` has at least `n` characters |
| `hasMaxLength` | `hasMaxLength(s, n)` | `s` has at most `n` characters |
| `countEquals` | `countEquals(t, n)` | table `t` has exactly `n` elements |
| `countGreaterThan` | `countGreaterThan(t, n)` | table `t` has more than `n` elements |
| `countLessThan` | `countLessThan(t, n)` | table `t` has fewer than `n` elements |
| `contains` | `contains(t, value)` | table `t` contains an element equal to `value` |

> **Note:** `matchesRegex` performs a substring check, not full pattern
> matching. For Lua patterns use the standard `string.find` / `string.match`
> directly, e.g. `string.match(email, "^[%w.]+@[%w.]+$") ~= nil`.

### Example

```cpp
auto rule = Rule::create(1, "isNotNull(customer) and inRange(customer.age, 18, 65)")
    .withName("eligible-age")
    .build();
```

## `Rule::` factory methods

Each returns a fully-formed `Rule` (by value) with the expression filled in.
The optional `description` is auto-generated when left empty.

| Factory | Signature | Generated expression |
|---------|-----------|----------------------|
| `Rule::isNotNull` | `(param, desc = "")` | `isNotNull(param)` |
| `Rule::greaterThan` | `(param, double value, desc = "")` | `param > value` |
| `Rule::lessThan` | `(param, double value, desc = "")` | `param < value` |
| `Rule::equals` | `(param, value, desc = "")` | `param == "value"` |
| `Rule::matchesRegex` | `(param, pattern, desc = "")` | `matchesRegex(param, "pattern")` |
| `Rule::contains` | `(param, substring, desc = "")` | substring check via `string.find` |

### Example

```cpp
// Equivalent to: Rule::create(id, "age > 18")
Rule adult = Rule::greaterThan("age", 18.0, "must be an adult");

Workflow workflow;
workflow.rules.push_back(std::make_shared<Rule>(std::move(adult)));
```

These factories cover the common cases; for anything richer, write the Lua
expression directly with `Rule::create` / `Rule::Builder` and the built-in
predicates above.
