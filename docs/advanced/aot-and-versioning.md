---
layout: default
title: AOT Compilation and Versioning
parent: Advanced Topics
nav_order: 1
---

# AOT Compilation and Rule Versioning

## AOT Compilation

FastRules supports Ahead-of-Time (AOT) compilation to pre-compile workflows into binary bundles. This eliminates compilation overhead at runtime.

### Creating an AOT Bundle

```cpp
#include <fastrules/aot_compiler.hpp>

LuaEngine engine;
Workflow workflow = Workflow::loadFromFile("rules.json", engine);
workflow.compile(engine);

// Serialize to binary
AotBundle bundle = AotBundle::compile(workflow, engine);
std::string binary = bundle.serialize();

// Save to file
bundle.saveToFile("rules.frb");
```

### Loading an AOT Bundle

```cpp
// Load pre-compiled workflow
AotBundle bundle = AotBundle::loadFromFile("rules.frb");
Workflow workflow = bundle.toWorkflow();

// Execute without compilation overhead
auto results = workflow.execute(engine, params);
```

### Bundle Format

The binary format includes:
- Magic header (`FRB\0`)
- Version number
- Workflow metadata
- Pre-compiled Lua bytecode for each rule
- Dependency graph

## Rule Versioning

FastRules supports semantic versioning for rules and workflows with full history tracking.

### Version Format

Versions follow SemVer: `MAJOR.MINOR.PATCH[-prerelease][+buildmetadata]`

Examples:
- `1.0.0` — Initial release
- `1.1.0` — Added new parameters (backward compatible)
- `2.0.0` — Breaking change
- `1.0.0-alpha+build.123` — Pre-release with build metadata

### Checking Compatibility

```cpp
RuleVersion v1(1, 0, 0);
RuleVersion v2(1, 1, 0);

// v1 is compatible with v2 (same major, v1 >= v2? No, but backward compat check is different)
// Actually: v2.IsCompatibleWith(v1) means v2 can replace v1
bool compatible = v2.IsCompatibleWith(v1); // true (same major, v2 > v1)
```

### Version History

```cpp
RuleVersionHistory history;
history.ruleId = "customer-check";

// Add versions
RuleVersion v1;
v1.versionId = "v1";
v1.expression = "age >= 18";
v1.createdAt = std::chrono::system_clock::now();
history.addVersion(v1);

// List all versions
auto versions = history.listVersions();

// Rollback to previous version
Rule oldRule = history.rollbackTo("v1");
```

### Version Manager

```cpp
RuleVersionManager manager;
manager.enableAutoSnapshot(true);

// Snapshot entire workflow
manager.snapshotWorkflow(workflow, "author", "Added age validation");

// Get history for a rule
auto history = manager.getHistory("customer-check");
if (history) {
    auto latest = history->getLatest();
}
```

### JSON Serialization

Version history can be exported/imported as JSON:

```cpp
std::string json = history.toJson();
auto restored = RuleVersionHistory::fromJson(json);
```
