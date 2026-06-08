# FastRules Persistence Extensions — Implementation Summary

## Overview

Three optional persistence extensions for FastRules, built as independent CMake projects. None are linked into core — developers opt-in by building what they need.

## Extension: fastrules-json

**Purpose:** JSON file-based persistence
**Dependencies:** nlohmann/json (already in core)
**Files:**
- `json/include/fastrules/json_repository.hpp`
- `json/src/json_repository.cpp`
- `json/tests/test_json_repository.cpp`

**Features:**
- CRUD operations for `Rule`
- Human-readable JSON format
- Flush-on-demand or auto-save on destruction
- Version-control friendly

**Build:**
```bash
cmake -B build -S extensions/json
cmake --build build
```

## Extension: fastrules-xml

**Purpose:** XML file-based persistence
**Dependencies:** pugixml (fetched via CMake)
**Files:**
- `xml/include/fastrules/xml_repository.hpp`
- `xml/src/xml_repository.cpp`
- `xml/tests/test_xml_repository.cpp`

**Features:**
- CRUD operations for `Rule`
- Structured XML with attributes for metadata, child nodes for collections
- `toString()` for debugging/transmission
- Enterprise-friendly format

**Build:**
```bash
cmake -B build -S extensions/xml
cmake --build build
```

## Extension: fastrules-db

**Purpose:** Database persistence via SOCI
**Dependencies:** SOCI + backend library (postgresql, mysql, sqlite3, odbc)
**Files:**
- `db/include/fastrules/db_repository.hpp`
- `db/src/db_repository.cpp`
- `db/tests/test_db_repository.cpp`

**Features:**
- CRUD operations with transactions
- Schema auto-creation (`createSchema()`)
- Supports PostgreSQL, MySQL, SQLite, SQL Server, Oracle
- Connection factory for easy setup
- Relational design: main `rules` table + `rule_parameters`, `rule_dependencies`, `rule_children`

**Build:**
```bash
# SQLite (for testing)
cmake -B build -S extensions/db -DFASTRULES_DB_BACKEND=sqlite3

# PostgreSQL (production)
cmake -B build -S extensions/db -DFASTRULES_DB_BACKEND=postgresql
```

## Architecture

All extensions implement the abstract repository interfaces:

```cpp
class IRuleRepository {
    virtual void save(const Rule& rule) = 0;
    virtual std::optional<Rule> findById(const std::string& id) = 0;
    virtual std::vector<Rule> findAll() = 0;
    virtual void remove(const std::string& id) = 0;
    virtual bool exists(const std::string& id) = 0;
    virtual size_t count() = 0;
};
```

This means:
- Swap JSON for DB without changing business logic
- Mock repositories for testing
- Easy to add new backends (YAML, Redis, etc.)

## Master Build

Build all extensions from the main project:

```bash
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
cmake --build build
```

Or selectively:

```bash
cmake -B build -S . \
    -DFASTRULES_BUILD_EXTENSIONS=ON \
    -DFASTRULES_BUILD_JSON_EXT=ON \
    -DFASTRULES_BUILD_XML_EXT=ON \
    -DFASTRULES_BUILD_DB_EXT=OFF
```

## CI Integration

Windows builds now test with and without extensions:
```yaml
matrix:
  extensions: [none, json-xml]
```

## Future Work

- `IWorkflowRepository` implementations (only `DbWorkflowRepository` has stub)
- `IVersionRepository` implementations
- Connection pooling for DB extension
- Async/await support for DB operations
- Migration scripts for schema versioning
