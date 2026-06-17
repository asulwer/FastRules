---
layout: default
title: Logging
nav_order: 9
---

# Logging

FastRules uses [spdlog](https://github.com/gabime/spdlog) for all internal logging. You configure spdlog once at application startup and all rule/workflow execution events flow through it.

## Basic Console Logging

```cpp
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    // Pattern: [HH:MM:SS.msec] [level] message
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");

    // Set global log level
    spdlog::set_level(spdlog::level::debug);

    // Create colored console logger
    auto console = spdlog::stdout_color_mt("fastrules");
    spdlog::set_default_logger(console);

    // Now all FastRules internals log here
    LuaEngine engine;
    // ... rules, workflows, execute ...
}
```

## File Logging

```cpp
#include <spdlog/sinks/basic_file_sink.h>

auto file_logger = spdlog::basic_logger_mt("fastrules", "logs/rules.log");
spdlog::set_default_logger(file_logger);
```

## Console + File (Multiple Sinks)

```cpp
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/rules.log", true);

std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks = {console_sink, file_sink};
auto logger = std::make_shared<spdlog::logger>("fastrules", sinks.begin(), sinks.end());
spdlog::set_default_logger(logger);
```

## Log Levels

| Level | Use |
|---|---|
| `trace` | Every expression evaluation, cache check, rule step |
| `debug` | Rule execution start/end, compilation, cache hit/miss |
| `info` | Workflow validation success, execution summary |
| `warn` | Rate limits, dependency skips, inactive rules |
| `error` | Rule exceptions, validation failures, timeouts |
| `critical` | Unknown exceptions, assertion failures |

## What Gets Logged

### Rule Execution

```
[18:32:45.123] [trace] Executing rule 1
[18:32:45.124] [trace] Evaluating expression for rule 1: age >= 18
[18:32:45.125] [debug] Rule 1 executed successfully
[18:32:45.126] [error] Rule 1 exception: division by zero
```

### Workflow Execution

```
[18:32:45.100] [debug] Executing workflow 1
[18:32:45.101] [info] Executing 3 rules in workflow 1
[18:32:45.130] [info] Workflow 1 executed — 3 results
```

### Child Rules

```
[18:32:45.120] [debug] Executing 2 child rules for rule 1
[18:32:45.127] [info] Child rule 2 failed — parent 1 aborted
```

### Validation

```
[18:32:44.900] [debug] Validating workflow 1
[18:32:44.901] [error] Duplicate rule ID detected: 5
[18:32:44.902] [info] Workflow 1 validated successfully
```

## Per-Engine Logger

If you need separate loggers for different engines:

```cpp
LuaEngine engine1;
auto engine1_log = spdlog::stdout_color_mt("engine1");
engine1.setLogger(engine1_log);

LuaEngine engine2;
auto engine2_log = spdlog::basic_logger_mt("engine2", "engine2.log");
engine2.setLogger(engine2_log);
```

## Complete Example

```cpp
#include <fastrules.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace fastrules;

int main() {
    // Console + file logging
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>("fastrules.log", true);
    auto logger = std::make_shared<spdlog::logger>("fastrules", std::initializer_list<std::shared_ptr<spdlog::sinks::sink>>{console, file});
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");

    LuaEngine engine;

    auto rule = Rule::create(1, "age >= 18").build();
    Workflow workflow;
    workflow.id = 1;
    workflow.rules = {rule};
    workflow.compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);

    auto results = workflow.execute(engine, params);

    spdlog::shutdown();
    return 0;
}
```
