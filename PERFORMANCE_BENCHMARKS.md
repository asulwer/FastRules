# FastRules Performance Benchmarks

This document summarizes the performance benchmarks for FastRules, with a focus on the improvements from regex removal in expression parsing.

## Overview

All benchmarks are run using Catch2's benchmarking support in **Release mode** with compiler optimizations enabled. Debug builds are skipped as they produce meaningless results.

## Expression Parsing Performance (Post-Regex Removal)

The expression parsing was refactored to use a hand-rolled parser instead of `std::regex`, resulting in significant performance improvements.

### Benchmark Results

| Benchmark | Mean Time | Description |
|-----------|-----------|-------------|
| `compile complex expression` | ~29.2 μs | Complex expression with multiple identifiers, operators, and literals |
| `compile 9 simple expressions (batch)` | ~34.0 μs | Batch compilation of 9 simple expressions |

### Complex Expression Tested

```lua
customer_age >= 18 and customer_age <= 65 and
(customer_tier == 'premium' or customer_tier == 'gold') and
order_total > 100.00 and (item_count > 5 or discount_code ~= 'VIP')
```

### Simple Expressions Tested (Batch)

```lua
x > 0
y < 100
z == 'active'
a ~= 'test'
b >= 10
c <= 50
d == true
e ~= 'pattern'
f > 3.14
```

## Rule Compilation Benchmarks

| Benchmark | Mean Time | Description |
|-----------|-----------|-------------|
| `compile simple rule` | TBD | Single rule with expression "x > 0 and y < 100" |
| `compile 10-rule workflow` | TBD | Workflow compilation with 10 independent rules |

## Rule Execution Benchmarks

| Benchmark | Mean Time | Description |
|-----------|-----------|-------------|
| `execute simple rule` | TBD | Single rule execution with simple expression |
| `execute rule with C++ type` | TBD | Rule using registered C++ type (Point struct) |

## Workflow Execution Benchmarks

| Benchmark | Mean Time | Description |
|-----------|-----------|-------------|
| `execute 5-rule sequential workflow` | TBD | Sequential execution of 5 rules |
| `execute 5-rule parallel workflow` | TBD | Parallel execution of 5 independent rules |

## Cache Performance

| Benchmark | Mean Time | Description |
|-----------|-----------|-------------|
| `execute cached rule (hit)` | TBD | Cached rule execution (cache hit) |
| `execute uncached rule` | TBD | Non-cached rule execution |

## Memory Benchmarks

| Benchmark | Description |
|-----------|-------------|
| `rule memory: create & compile` | Memory footprint for rule creation |
| `workflow memory: execute 50 rules` | Memory scaling for 50-rule workflow |

## Performance Guidelines

### Expression Compilation

- **Complex expressions**: ~29 μs per expression (fast with hand-rolled parser)
- **Batch compilation**: ~34 μs for 9 expressions (~3.8 μs per expression)
- The hand-rolled parser provides significant speedup over `std::regex`

### Recommendations

1. **Use caching**: Enable `cacheDuration` on frequently executed rules
2. **Batch compilation**: Compile multiple rules together when possible
3. **Prefer parallel execution**: Use `executeParallel()` or `AsyncWorkflow` for independent rules
4. **Avoid repeated compilation**: Compile once, execute many times

## Running Benchmarks

```bash
# Run all benchmarks
./fastrules_tests "[benchmark]"

# Run specific benchmark category
./fastrules_tests "[benchmark][compilation]"
./fastrules_tests "[benchmark][execution]"
./fastrules_tests "[benchmark][parsing]"
./fastrules_tests "[benchmark][cache]"
./fastrules_tests "[benchmark][memory]"
```

## Technical Notes

- Benchmarks use Catch2's `BENCHMARK` macro
- Each benchmark runs multiple samples and iterations for statistical significance
- Results shown are **mean** times with standard deviation
- All times are in microseconds (μs) unless otherwise noted

## Comparison: Regex vs Hand-Rolled Parser

The regex-based expression parsing was replaced with a hand-rolled parser for better performance:

| Aspect | Regex Approach | Hand-Rolled Parser |
|--------|---------------|-------------------|
| **Complex Expression** | Slower | ~29 μs |
| **Code Complexity** | Lower (regex patterns) | Higher (state machine) |
| **Maintainability** | Harder to debug | Easier to extend |
| **Compile Time** | Heavy regex headers | Lighter includes |

The hand-rolled parser provides better performance while maintaining readability and debuggability.

## Future Improvements

Potential areas for further optimization:

1. **Expression pre-compilation cache**: Cache compiled expressions across runs
2. **Parallel compilation**: Compile independent rules in parallel
3. **Memory pool allocation**: Reduce allocation overhead for rule results
4. **SIMD optimizations**: Vectorized evaluation for numeric operations

---

*Last updated: 2026-06-11*
*Benchmarks run on: Windows x64, Release build*
