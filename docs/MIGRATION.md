---
layout: default
title: Migration from RoslynRules
nav_order: 9
---

# Migrating from RoslynRules

## Expression Language

| RoslynRules | FastRules |
|-------------|-----------|
| `customer.Age > 18` | `customer.age > 18` |
| `order.Total > 100 && order.Items.Count > 5` | `order.total > 100 and #order.items > 5` |
| `customer.Name.StartsWith("A")` | `string.sub(customer.name, 1, 1) == "A"` |
| `items.Any(i => i.Price > 10)` | Custom predicate or loop |

## API Mapping

| RoslynRules | FastRules |
|-------------|-----------|
| `Rule("id", "expr")` | `auto r = make_shared<Rule>(); r->id = "id"; r->expression = "expr";` |
| `rule.Compile()` | `rule->compile(engine);` |
| `rule.Execute(params)` | `rule->execute(engine, ctx, params);` |
| `workflow.Execute(params)` | `workflow.execute(engine, params);` |
| `workflow.ExecuteAsync(params)` | `async.executeParallelAsync(engine, params);` |
| `workflow.ExecuteStreaming(params)` | `workflow.executeStreaming(engine, params);` |
| `JsonRuleLoader.LoadWorkflow()` | `JsonLoader::loadWorkflow(json);` |
| `RuleParameter("name", typeof(int), value)` | `RuleParameter("name", "int", any(value));` |

## Type Registration

**RoslynRules (C#):**
```csharp
RuleParameter("customer", typeof(Customer))
// Expression: customer.Age > 18
```

**FastRules (C++):**
```cpp
engine.registerType<Customer>("Customer", [](auto& ut) {
    ut["age"] = &Customer::age;
});
params.emplace_back("customer", "Customer", customer);
// Expression: customer.age > 18
```

## Predicates

| RoslynRules | FastRules |
|-------------|-----------|
| `IsNotNull(x)` | `isNotNull(x)` |
| `GreaterThan(x, 10)` | `x > 10` |
| `LessThan(x, 10)` | `x < 10` |
| `Equals(x, "val")` | `x == "val"` |
| `MatchesRegex(x, "pattern")` | `matchesRegex(x, "pattern")` |
| `Contains(x, "sub")` | `string.find(x, "sub") ~= nil` |

## Key Differences

1. **Lua vs C# syntax** — Lua uses `and`/`or`/`not`, `#` for length, `~=` for not-equals
2. **Case sensitivity** — Lua is case-sensitive; property names must match exactly
3. **No LINQ** — Use Lua tables and loops instead
4. **Memory** — FastRules is ~2MB vs RoslynRules ~50MB
5. **Compilation** — Lua compiles in ~1ms vs Roslyn ~50ms first time
