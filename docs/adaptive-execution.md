---
layout: default
title: Adaptive Execution
nav_order: 7
---

# Adaptive Execution

FastRules provides intelligent execution mode selection through the `executeAdaptive()` method.

## Overview

The `executeAdaptive()` method automatically chooses between sequential and parallel execution based on workflow characteristics and runtime performance measurements.

## Configuration

### Manual Threshold

Set a fixed threshold for switching between sequential and parallel:

```cpp
Workflow workflow;

// Sequential for <= 4 rules, parallel for > 4 rules
workflow.setAdaptiveThreshold(4);

// Always use sequential (avoid thread overhead)
workflow.setAdaptiveThreshold(0);

// Always use parallel (maximize concurrency)
workflow.setAdaptiveThreshold(SIZE_MAX);
```

### Dynamic Auto-Detection

Enable automatic threshold optimization based on actual performance:

```cpp
// Enable auto-detection
workflow.enableAutoDetection(true);

// Execute - system learns optimal threshold
auto results = workflow.executeAdaptive(engine, params);

// Check current threshold (auto-adjusted)
std::cout << "Current threshold: " << workflow.getAdaptiveThreshold() << "\n";

// Check performance statistics
std::cout << "Avg sequential time: " << workflow.getSequentialAvgTime() << " µs\n";
std::cout << "Avg parallel time: " << workflow.getParallelAvgTime() << " µs\n";
```

## How Dynamic Detection Works

1. **Sampling**: Every 100 executions, both strategies are tested
2. **Measurement**: Actual execution times are recorded (microseconds)
3. **Averaging**: Rolling averages maintained for both strategies
4. **Adjustment**: Threshold adjusted based on which strategy is faster
   - Sequential > 20% faster → increase threshold
   - Parallel > 20% faster → decrease threshold
5. **Bounds**: Threshold stays within 2-20 rules (prevents extremes)

## Performance Guidelines

Based on benchmarks:

| Rule Count | Recommended Mode | Typical Performance |
|------------|------------------|---------------------|
| 1-2 rules | Sequential | ~20 µs |
| 3-5 rules | Adaptive* | 20-40 µs |
| 6+ rules | Parallel | ~25-30 µs |

*Adaptive mode automatically chooses based on measured performance

## When to Use Auto-Detection

**Enable when:**
- Workload varies significantly
- Running on different hardware (unknown core count)
- Rules have varying complexity
- Performance is critical

**Disable when:**
- Workload is consistent and known
- Minimal overhead required (skip sampling)
- Specific threshold already determined by testing

## Example Usage

```cpp
#include <fastrules.hpp>

using namespace fastrules;

int main() {
    LuaEngine engine;
    Workflow workflow;
    
    // Add rules...
    for (int i = 0; i < 10; ++i) {
        auto rule = std::make_shared<Rule>();
        rule->id = i;
        rule->expression = "x > " + std::to_string(i);
        workflow.rules.push_back(rule);
    }
    
    workflow.compile(engine);
    
    // Enable auto-detection for optimal performance
    workflow.enableAutoDetection(true);
    
    // Execute - automatically uses best strategy
    std::vector<RuleParameter> params;
    params.emplace_back("x", 42);
    
    auto results = workflow.executeAdaptive(engine, params);
    
    // After many executions, check what was learned
    std::cout << "Optimal threshold for this workload: " 
              << workflow.getAdaptiveThreshold() << "\n";
    
    return 0;
}
```

## API Reference

### Workflow Methods

| Method | Description |
|--------|-------------|
| `executeAdaptive(engine, params)` | Execute with automatic mode selection |
| `setAdaptiveThreshold(n)` | Set manual threshold (0 = always sequential) |
| `getAdaptiveThreshold()` | Get current threshold |
| `enableAutoDetection(bool)` | Enable/disable auto-detection |
| `isAutoDetectionEnabled()` | Check if auto-detection is active |
| `getSequentialAvgTime()` | Get average sequential execution time |
| `getParallelAvgTime()` | Get average parallel execution time |

## See Also

- [Parallel Execution](parallel-execution.md) - Manual parallel execution
- [Performance Tuning](performance.md) - General optimization guide
