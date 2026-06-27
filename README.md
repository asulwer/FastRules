# FastRules

[![CI](https://github.com/asulwer/fastrules/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/asulwer/fastrules/actions/workflows/ci.yml)
[![Coverage](https://codecov.io/gh/asulwer/fastrules/branch/main/graph/badge.svg)](https://codecov.io/gh/asulwer/fastrules)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://asulwer.github.io/FastRules)

High-performance **C++23 business rules engine** with **Lua expressions**. The
core library is dependency-light; optional persistence extensions add JSON, XML,
or database support as needed.

> A C++ rewrite of [RoslynRules](https://github.com/asulwer/roslynrules) with
> Lua instead of C#/Roslyn compilation.

| | [RoslynRules](https://github.com/asulwer/roslynrules) | FastRules |
|---|---|---|
| **Language** | C# | C++23 |
| **Expression engine** | Roslyn compiler → IL | Lua → bytecode |
| **Compile time** | ~50ms first, cached after | ~1ms |
| **Memory** | ~50MB | ~2MB |
| **Deployment** | .NET runtime required | Self-contained |
| **Sandboxing** | Assembly whitelist | Lua environment removal |

## Requirements

- CMake 3.28+
- C++23 compiler (GCC 13+, Clang 17+, MSVC 2022+)
- Git (for FetchContent dependencies)

Core dependencies (`lua`, `spdlog`) are resolved automatically via vcpkg
manifest mode; LuaBridge3 is fetched via CMake FetchContent. No manual setup
required.

## Getting Started

### 1. Build

```bash
git clone https://github.com/asulwer/fastrules.git
cd fastrules
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON -DFASTRULES_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

### 2. Write your first rule

```cpp
#include <fastrules.hpp>
#include <iostream>

using namespace fastrules;

int main() {
    LuaEngine engine;

    // A rule is a Lua condition + an optional Lua action.
    auto rule = Rule::Builder(1)
        .withExpression("age >= 18")
        .withAction("status = 'adult'")
        .build();

    Workflow workflow;
    workflow.rules.push_back(rule);
    workflow.compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);          // becomes a Lua global

    auto results = workflow.execute(engine, params);
    std::cout << (results[0].isSuccess() ? "passed" : "failed") << "\n";
}
```

That's the whole loop: **build a rule → compile a workflow → execute with
parameters**. Parameters become Lua globals (or typed objects), expressions
decide pass/fail, and actions run on success.

### 3. Load rules from a file (optional)

Need rules as configuration instead of code? Build with extensions and load
from JSON, XML, or a database:

```bash
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
```

```cpp
#include <fastrules.hpp>
#include <fastrules/json_loader.hpp>

auto workflow = fastrules::JsonLoader::loadWorkflow(readFile("rules.json"));
```

```json
{
  "id": 1,
  "version": "1.0.0",
  "rules": [
    { "id": 1, "expression": "customer.age >= 18", "action": "eligible = true", "priority": 0 }
  ]
}
```

## At a Glance

- **Rules & workflows** — condition/action pairs with priority, dependencies,
  and child rules.
- **Lua expressions** — fast compile (~1ms), self-contained, no runtime needed.
- **C++ interop** — register structs and enums for field access in Lua;
  register C++ callbacks for Lua actions.
- **Parallel & async** — `executeParallel`, async workflows, streaming results.
- **Production features** — caching with TTL, timeouts, rate limiting,
  security sandboxing, structured logging, execution tracing.
- **Persistence** — JSON, XML, and database (SOCI) extensions, plus AOT
  compilation and rule versioning.
- **Embeddable** — C API powers the C# and Python FFI examples.

## Documentation

Full documentation lives at **[asulwer.github.io/FastRules](https://asulwer.github.io/FastRules)**
and in the [`docs/`](docs/) directory:

| Topic | Description |
|---|---|
| [Getting Started](docs/getting_started.md) | Install, build options, your first rule |
| [Architecture](docs/architecture.md) | Core design and component overview |
| [Core Concepts](docs/concepts.md) | Rules, workflows, contexts, results |
| [Examples](docs/examples.md) | Guide to the ~20 runnable examples |
| [API Reference](docs/api/) | Rule, Workflow, LuaEngine, type registry, callbacks |
| [Parallel Execution](docs/parallel-execution.md) | `executeParallel` vs `AsyncWorkflow` |
| [Predicates](docs/predicates.md) | Built-in predicate factories |
| [Security](docs/security.md) | Sandboxing and expression validation |
| [Observability](docs/observability.md) | Tracing, performance counters, logging |
| [Extensions](docs/extensions/) | JSON, XML, and database persistence |
| [Advanced](docs/advanced/) | AOT compilation, versioning, custom methods |
| [C API](docs/c_api.md) | Embedding from C, C#, and Python |
| [Migration](docs/migration.md) | Migrating from RoslynRules |
| [Troubleshooting](docs/troubleshooting.md) | Common issues and fixes |

## License

MIT
