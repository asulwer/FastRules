---
layout: default
title: Database Extension Setup
parent: Extensions
nav_order: 4
---

# Database Extension Setup Guide

## Overview

The `fastrules-db` extension provides database persistence for rules and workflows using [SOCI](https://soci.sourceforge.net/) — a C++ database access library. Supports PostgreSQL, MySQL, SQLite, Oracle, and ODBC.

## Prerequisites

### Install SOCI via vcpkg (Recommended)

SOCI is required to build the DB extension. The easiest way to install it on all platforms is via [vcpkg](https://vcpkg.io/).

**Windows (x64):**
```bash
vcpkg install soci[soci-core,sqlite3] --triplet x64-windows
```

**Linux/macOS:**
```bash
vcpkg install soci[soci-core,sqlite3]
```

> **Note:** The `soci-core` feature is required for the core SOCI library. `sqlite3` is the most common backend for local development and testing. Add `postgresql` or `mysql` features if you need those backends:
> ```bash
> vcpkg install soci[soci-core,sqlite3,postgresql] --triplet x64-windows
> ```

### Alternative: Conan

```bash
conan install -r conancenter soci/4.0.3
```

### Alternative: System Package Managers

**Ubuntu/Debian:**
```bash
sudo apt-get install libsoci-dev libsoci-sqlite3-dev
```

**macOS:**
```bash
brew install soci
```

**Windows (manual build):**
1. Download SOCI from https://github.com/SOCI/soci/releases
2. Build with CMake:
   ```bash
   cmake -B build -S . -DSOCI_CXX11=ON -DWITH_SQLITE3=ON
   cmake --build build --config Release
   cmake --install build --prefix C:/soci
   ```

### Install Database Backends

**SQLite:** Usually included with SOCI or available system-wide.

**PostgreSQL:**
```bash
# Ubuntu
sudo apt-get install libpq-dev

# macOS
brew install libpq

# Windows (vcpkg)
vcpkg install libpq --triplet x64-windows
```

**MySQL:**
```bash
# Ubuntu
sudo apt-get install libmysqlclient-dev

# macOS
brew install mysql-client
```

---

## Build Configuration

### CMake with vcpkg Toolchain

When using vcpkg, pass the vcpkg toolchain file to CMake:

**Windows:**
```bash
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DFASTRULES_BUILD_EXTENSIONS=ON ^
    -DFASTRULES_BUILD_DB=ON
```

**Linux/macOS:**
```bash
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DFASTRULES_BUILD_EXTENSIONS=ON \
    -DFASTRULES_BUILD_DB=ON
```

### CMake without vcpkg (manual SOCI path)

If SOCI is not found, set the path explicitly:

```bash
# Ubuntu system install
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON -DFASTRULES_BUILD_DB=ON -DSOCI_ROOT=/usr

# Manual install
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON -DFASTRULES_BUILD_DB=ON -DSOCI_ROOT=C:/soci
```

Or set the CMake config directory directly:
```bash
cmake -B build -S . -DSOCI_DIR=/usr/local/lib/cmake/SOCI
```

### Environment Variables

```bash
export SOCI_ROOT=/usr/local      # or C:\soci on Windows
export PATH=$SOCI_ROOT/bin:$PATH
```

### vcpkg.json Feature

The `vcpkg.json` in the FastRules root defines the `db` feature with the SOCI dependency:

```json
{
  "features": {
    "db": {
      "description": "Database persistence via SOCI",
      "dependencies": ["soci[soci-core,sqlite3]"]
    }
  }
}
```

Install all features with:
```bash
vcpkg install --triplet x64-windows
```

---

## The `DbBool` Abstraction

SOCI does **not** natively support `bool` in its `exchange_type` enum. To work around this limitation, fastrules-db stores boolean values as `int(1)` in the database (`0` = false, `1` = true) and provides a `DbBool` wrapper with a custom SOCI type conversion.

### How It Works

Internally, `db_bool.hpp` defines a `DbBool` struct with a custom `soci::type_conversion` specialization:

```cpp
#include <fastrules/db_bool.hpp>

fastrules::ext::DbBool isActive = true;  // Stores as int(1) in DB
fastrules::ext::DbBool isActive = false; // Stores as int(0) in DB
```

- `DbBool` wraps a `bool value`
- On **write** (`to_base`): converts `bool` → `int` (`1` or `0`)
- On **read** (`from_base`): converts `int` → `bool` (`!= 0` → `true`)
- Null indicators default to `false`

You typically do **not** need to use `DbBool` directly — the `DbRuleRepository`, `DbWorkflowRepository`, and `DbVersionRepository` classes handle the conversion automatically for fields like `isActive`.

---

## Usage

### Connect to Database

```cpp
#include <fastrules/db_repository.hpp>

// SQLite (file-based, zero setup)
auto session = fastrules::ext::DbConnectionFactory::create(
    "sqlite3",              // backend name
    "rules.db"              // connection string (filename for SQLite)
);

// PostgreSQL
auto session = fastrules::ext::DbConnectionFactory::create(
    "postgresql",
    "dbname=rules host=localhost user=fastrules password=secret"
);

// MySQL
auto session = fastrules::ext::DbConnectionFactory::create(
    "mysql",
    "db=rules;host=localhost;user=fastrules;password=secret"
);
```

### Schema Auto-Creation

The repository creates tables automatically on first use:

```cpp
fastrules::ext::DbRuleRepository repo(session);
// Schema is created when you call save() for the first time
```

### Save and Load Rules (Builder Pattern)

Use the modern `Rule::builder()` API:

```cpp
// Create a rule using the builder
auto rule = Rule::builder("fraud-check")
    .withExpression("amount < 10000")
    .withAction("flagged = false")
    .withDescription("Fraud detection rule")
    .withParameterNames({"amount"})
    .withPriority(100)
    .active(true)
    .build();

// Save
repo.save(*rule);

// Load by ID
auto loaded = repo.findById("fraud-check");
if (loaded) {
    std::cout << loaded->expression << "\n";
}

// List all
auto all = repo.findAll();
for (const auto& r : all) {
    std::cout << r.id << " (priority=" << r.priority << ")\n";
}
```

### Full Example

See `extensions/db/examples/db_example.cpp` for a complete working example that demonstrates:

- SQLite connection setup
- Builder-pattern rule creation
- CRUD operations (`save`, `findById`, `findAll`)
- Updating existing rules
- Executing a `Workflow` using database-backed rules
- PostgreSQL connection (commented)

---

## CMakeLists.txt Snippet

To use the DB extension in your project:

```cmake
find_package(fastrules REQUIRED)
find_package(fastrules-db REQUIRED)
find_package(SOCI REQUIRED COMPONENTS core sqlite3)

target_link_libraries(myapp
    PRIVATE
        fastrules::fastrules
        fastrules::fastrules-db
        SOCI::soci_core
)
```

---

## Copying DLLs on Windows

When building and running on Windows with SOCI installed via vcpkg, the following DLLs may need to be copied to your executable's output directory (e.g., `build/Debug/` or `build/Release/`):

| DLL | Source (vcpkg) | Purpose |
|-----|----------------|---------|
| `soci_core_4_0.dll` | `installed/x64-windows/bin/` | SOCI core library |
| `soci_sqlite3_4_0.dll` | `installed/x64-windows/bin/` | SQLite backend for SOCI |
| `sqlite3.dll` | `installed/x64-windows/bin/` | SQLite3 engine |

**Quick copy command (PowerShell):**
```powershell
$vcpkgBin = "C:\vcpkg\installed\x64-windows\bin"
$outDir = ".\build\Release"  # or Debug

Copy-Item "$vcpkgBin\soci_core_4_0.dll" $outDir
Copy-Item "$vcpkgBin\soci_sqlite3_4_0.dll" $outDir
Copy-Item "$vcpkgBin\sqlite3.dll" $outDir
```

Alternatively, add the vcpkg `bin` directory to your `PATH` environment variable.

---

## Troubleshooting

### SOCI not found by CMake

1. Verify SOCI installation:
   ```bash
   # Windows (vcpkg)
   dir C:\vcpkg\installed\x64-windows\lib\cmake\SOCI

   # Linux/macOS
   ls /usr/local/lib/cmake/SOCI
   ```
   You should see `SOCIConfig.cmake` and related files.

2. If using vcpkg, ensure the toolchain file is passed to CMake:
   ```bash
   -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

3. Set explicit SOCI path:
   ```bash
   cmake -B build -S . -DSOCI_DIR=/usr/local/lib/cmake/SOCI
   ```

### Runtime error: "Backend not found"

SOCI loads database backends as shared libraries. Ensure they're in your library path:

```bash
# Linux/macOS
export LD_LIBRARY_PATH=$SOCI_ROOT/lib:$LD_LIBRARY_PATH

# Windows
# Add C:\vcpkg\installed\x64-windows\bin to your PATH
# OR copy DLLs next to your executable (see "Copying DLLs on Windows" above)
```

### SQLite "database locked" errors

- Use WAL mode for concurrent access:
  ```sql
  PRAGMA journal_mode=WAL;
  ```
- Avoid multiple processes writing simultaneously
- Use connection pooling for threaded applications

### Build error: "cannot find -lsoci_sqlite3" or "SOCI::soci_sqlite3 target not found"

The DB extension's `CMakeLists.txt` attempts to locate the SQLite backend library in multiple ways:

1. **vcpkg path** (`${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib`)
2. **CMake prefix path**
3. **Hardcoded vcpkg paths** (`C:/tools/vcpkg/installed/x64-windows/lib`)
4. **CMake target** `SOCI::soci_sqlite3`

If none are found, the build will still succeed but runtime backend loading may fail. To fix:

- Ensure you installed SOCI with the `sqlite3` feature: `vcpkg install soci[soci-core,sqlite3]`
- Set `CMAKE_PREFIX_PATH` to your vcpkg installation:
  ```bash
  cmake -B build -S . -DCMAKE_PREFIX_PATH=C:/vcpkg/installed/x64-windows
  ```

### Test failures: "unable to open database file" or "table does not exist"

- Ensure the test binary has write permissions to the temp directory
- The schema auto-creates on first `save()` — do not query before saving
- Remove stale test databases: `del %TEMP%\test_rules.db`

### "Bool type not supported" or similar SOCI exchange errors

This happens if code attempts to bind a raw `bool` directly to a SOCI statement instead of using `DbBool`. The fastrules-db repositories already handle this internally. If you write custom SOCI queries alongside fastrules, use `fastrules::ext::DbBool` for boolean columns.

---

## Quick Reference

| Task | Command |
|------|---------|
| Install SOCI (vcpkg) | `vcpkg install soci[soci-core,sqlite3] --triplet x64-windows` |
| Configure build | `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DFASTRULES_BUILD_EXTENSIONS=ON -DFASTRULES_BUILD_DB=ON` |
| Build | `cmake --build build --config Release` |
| Run DB tests | `ctest -C Release --output-on-failure -R db` |
| Copy Windows DLLs | See "Copying DLLs on Windows" above |
