---
layout: default
title: Database Extension
parent: Extensions
nav_order: 3
---

# Database Extension

The `fastrules-db` extension persists rules and workflows to a relational database via [SOCI](https://soci.sourceforge.net/). Supports PostgreSQL, MySQL, SQLite, Oracle, and ODBC backends.

## Installation

### Prerequisites

Install SOCI via vcpkg:

**Windows:**
```bash
vcpkg install soci[soci-core,sqlite3] --triplet x64-windows
```

**Linux/macOS:**
```bash
vcpkg install soci[soci-core,sqlite3]
```

### CMake

```bash
cmake -B build -S . \
    -DFASTRULES_BUILD_EXTENSIONS=ON \
    -DFASTRULES_BUILD_DB=ON
```

```cmake
target_link_libraries(your_target fastrules fastrules-db)
```

## Loading a Workflow from Database

```cpp
#include <fastrules/db_repository.hpp>
#include <fastrules.hpp>

int main() {
    LuaEngine engine;

    // Connect to database
    fastrules::DbRepository repo("sqlite3://rules.db");

    // Load workflow by ID
    auto workflow = repo.loadWorkflow("customer-validation");
    workflow.compile(engine);

    // Execute
    std::vector<RuleParameter> params;
    params.emplace_back("age", 25);
    params.emplace_back("name", std::string("Alice"));

    auto results = workflow.execute(engine, params);
    return 0;
}
```

## Saving a Workflow to Database

```cpp
// Create workflow
Workflow workflow;
workflow.id = "customer-validation";
workflow.description = "Validate new customers";
// ... add rules ...

// Save
repo.saveWorkflow(workflow);
```

## Database Schema

The extension creates two tables:

```sql
CREATE TABLE workflows (
    id TEXT PRIMARY KEY,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE rules (
    id TEXT PRIMARY KEY,
    workflow_id TEXT REFERENCES workflows(id),
    expression TEXT NOT NULL,
    action TEXT,
    priority INTEGER DEFAULT 0,
    active INTEGER DEFAULT 1,
    depends_on_rule_id TEXT,
    timeout_ms INTEGER,
    cache_duration_ms INTEGER
);
```

## Supported Backends

| Backend | Connection String | Notes |
|---|---|---|
| SQLite | `sqlite3://path/to/db.sqlite` | File-based, zero config |
| PostgreSQL | `postgresql://user:pass@host/db` | Production recommended |
| MySQL | `mysql://user:pass@host/db` | Widely supported |
| ODBC | `odbc://DSN` | Windows/enterprise |
| Oracle | `oracle://user:pass@host/service` | Enterprise |

## Error Handling

```cpp
try {
    auto workflow = repo.loadWorkflow("customer-validation");
} catch (const fastrules::DbConnectionException& e) {
    std::cerr << "DB connection failed: " << e.what() << "\n";
} catch (const fastrules::DbNotFoundException& e) {
    std::cerr << "Workflow not found: " << e.what() << "\n";
}
```

## See Also

- [Database Extension Setup Guide](../db_extension_setup.md) — detailed SOCI installation steps
