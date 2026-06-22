---
layout: default
title: Observability
nav_order: 9
---

# Observability

FastRules ships three complementary observability tools:

- **Execution tracing** — a per-execution timeline of every stage each rule went
  through.
- **Performance counters** — process-wide aggregate metrics (throughput, success
  rate, cache hit rate).
- **Structured logging** — see the separate [Logging guide](logging.html).

## Execution Tracing

`ExecutionTracer` records a step for each stage of rule execution (compile,
evaluate, action, skip, dependency_check). Pass one to
`Workflow::executeWithTrace`:

```cpp
#include <fastrules/execution_tracer.hpp>

ExecutionTracer tracer(workflow.id);   // constructor takes the workflow id (int)
auto results = workflow.executeWithTrace(engine, params, tracer);

const ExecutionTrace& trace = tracer.getTrace();
std::cout << "Total: " << trace.totalDuration().count() << "ns, "
          << trace.steps.size() << " steps\n";
```

### Inspecting a trace

`ExecutionTrace` exposes a few analysis helpers:

| Method | Returns |
|--------|---------|
| `totalDuration()` | wall-clock time for the whole execution |
| `getStepsForRule(name)` | all steps recorded for one rule |
| `getTotalTimeInStage(stage)` | summed time across rules in a stage (e.g. `"evaluate"`) |
| `getSlowestStep()` | the single longest step (`std::optional`) |

```cpp
if (auto slowest = trace.getSlowestStep()) {
    std::cout << "Slowest: " << slowest->ruleName
              << " (" << slowest->stage << ") "
              << slowest->duration().count() << "ns\n";
}

auto evalTime = trace.getTotalTimeInStage("evaluate");
```

Each `ExecutionTraceStep` carries the `ruleName`, `stage`, `success`,
`startedAt`/`endedAt` timestamps (with a `duration()` helper), and optional
`message`, `expression`, and `action` detail.

> Use tracing for profiling and debugging individual executions — it allocates a
> step per stage, so prefer the lighter performance counters for always-on
> aggregate metrics.

## Performance Counters

`PerformanceCounters` is a process-wide singleton that the engine updates as
rules execute. It is cheap (atomic increments) and safe to read at any time.

```cpp
#include <fastrules/performance_counters.hpp>

auto& pc = PerformanceCounters::instance();

// ... run workflows ...

auto c = pc.getCounters();   // snapshot (Counters)
std::cout << "Executed:  " << c.totalRulesExecuted   << "\n"
          << "Succeeded: " << c.totalRulesSuccessful << "\n"
          << "Failed:    " << c.totalRulesFailed     << "\n"
          << "Cached:    " << c.totalRulesCached      << "\n"
          << "Timed out: " << c.totalRulesTimedOut    << "\n"
          << "Throttled: " << c.totalRulesRateLimited << "\n";

// Derived metrics
std::cout << "Avg exec:  " << pc.getAverageExecutionTimeMs() << "ms\n"
          << "Success %: " << pc.getSuccessRate()  * 100 << "\n"
          << "Cache hit %: " << pc.getCacheHitRate() * 100 << "\n";

pc.reset();   // zero all counters (e.g. between benchmark runs)
```

### Tracked counters

`totalRulesExecuted`, `totalRulesSuccessful`, `totalRulesFailed`,
`totalRulesSkipped`, `totalRulesCached`, `totalRulesTimedOut`,
`totalRulesRateLimited`, `totalCompileCount`, `totalCompileFailures`, and
`totalExecutionTimeNs`.

Because the counters are global, reset them at known boundaries when you want
per-phase numbers rather than process lifetime totals.
