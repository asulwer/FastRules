---
layout: default
title: Installation
nav_order: 2
has_children: false
---

# Installation

## Requirements

| Tool | Minimum Version | Notes |
|---|---|---|
| CMake | 3.28+ | FetchContent for dependencies |
| C++ Compiler | C++23 | Visual Studio 2022, GCC 13+, Clang 17+ |
| Git | 2.30+ | For submodule-like FetchContent |
| Python | 3.9+ | Only for running benchmarks |

## Quick Start (Default Build)

```bash
git clone https://github.com/asulwer/fastrules.git
cd fastrules
cmake -B build -S .
cmake --build build --config Release
ctest --output-on-failure
```

Core library (`fastrules`) builds with zero manual dependency installation. CMake FetchContent downloads sol2 and nlohmann/json automatically.

---

## Platform-Specific Instructions

### Windows (Visual Studio 2022)

**Option A: Visual Studio IDE**
1. Open folder in VS 2022 (File → Open → CMake)
2. Select configuration: `x64-Release` or `x64-Debug`
3. Build → Build All
4. Test → Run CTest

**Option B: Command Line**
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

**Option C: PowerShell Script**
```powershell
.\build.ps1 -Clean
```
Builds in `build_vs/` with all extensions enabled.

### Linux (Ubuntu/Debian)

```bash
# Install build tools
sudo apt update
sudo apt install cmake g++ git

# Build
git clone https://github.com/asulwer/fastrules.git
cd fastrules
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Build
git clone https://github.com/asulwer/fastrules.git
cd fastrules
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(sysctl -n hw.ncpu)
```

---

## Package Managers

### vcpkg (Recommended)

```bash
vcpkg install fastrules
```

Link in your `CMakeLists.txt`:
```cmake
find_package(fastrules CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE fastrules)
```

### Conan

```bash
conan install fastrules/0.1.0
```

Link:
```cmake
find_package(fastrules REQUIRED)
target_link_libraries(your_app PRIVATE fastrules::fastrules)
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    fastrules
    GIT_REPOSITORY https://github.com/asulwer/fastrules.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(fastrules)

target_link_libraries(your_app PRIVATE fastrules)
```

---

## Build Options

| Option | Default | Description |
|---|---|---|
| `FASTRULES_BUILD_TESTS` | `ON` | Catch2 test suite |
| `FASTRULES_BUILD_EXAMPLES` | `ON` | Example programs |
| `FASTRULES_BUILD_EXTENSIONS` | `OFF` | JSON, XML, DB extensions |
| `FASTRULES_BUILD_DB` | `OFF` | Database extension (requires SOCI) |
| `FASTRULES_BUILD_COVERAGE` | `OFF` | gcov/llvm-cov coverage |
| `FASTRULES_LUA_BACKEND` | `sol2` | `sol2` or `luabridge3` |
| `FASTRULES_USE_LUAJIT` | `OFF` | Use LuaJIT instead of PUC-Rio Lua |

### Minimal Build (Core Only)

```bash
cmake -B build -S . \
    -DFASTRULES_BUILD_TESTS=OFF \
    -DFASTRULES_BUILD_EXAMPLES=OFF \
    -DFASTRULES_BUILD_EXTENSIONS=OFF
cmake --build build
```

### Build with Extensions

```bash
# JSON + XML (no external deps)
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON

# JSON + XML + DB (requires SOCI)
cmake -B build -S . \
    -DFASTRULES_BUILD_EXTENSIONS=ON \
    -DFASTRULES_BUILD_DB=ON
```

### LuaBridge3 Backend

```bash
cmake -B build -S . -DFASTRULES_LUA_BACKEND=luabridge3
```

---

## Extensions

### JSON Extension

No manual dependency installation required. CMake FetchContent handles nlohmann/json.

```bash
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON
```

Link: `target_link_libraries(your_app PRIVATE fastrules fastrules-json)`

### XML Extension

No manual dependency installation required. CMake FetchContent handles pugixml.

Link: `target_link_libraries(your_app PRIVATE fastrules fastrules-xml)`

### Database Extension

Requires SOCI. Install via vcpkg:

**Windows:**
```powershell
vcpkg install soci[soci-core,sqlite3] --triplet x64-windows
```

**Linux/macOS:**
```bash
vcpkg install soci[soci-core,sqlite3]
```

Then build:
```bash
cmake -B build -S . \
    -DFASTRULES_BUILD_EXTENSIONS=ON \
    -DFASTRULES_BUILD_DB=ON \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

Link: `target_link_libraries(your_app PRIVATE fastrules fastrules-db)`

See [Database Extension Setup](extensions/db.md) for detailed SOCI installation.

---

## Troubleshooting

### "Could not find sol2"

FetchContent failed to download. Check network connectivity, then:
```bash
rm -rf build/_deps
 cmake -B build -S .
```

### "Lua headers not found"

CMake found Lua but not headers. Set explicitly:
```bash
cmake -B build -S . -DLUA_INCLUDE_DIR=/usr/include/lua5.4
```

### "SOCI not found" (DB extension)

SOCI is not in the search path. Install via vcpkg and pass toolchain:
```bash
cmake -B build -S . \
    -DFASTRULES_BUILD_DB=ON \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

### Tests Fail on Windows

Some tests require terminal UI. Skip interactive tests:
```bash
cmake -B build -S . -DFASTRULES_BUILD_TESTS=ON
cmake --build build --config Release
ctest -C Release --output-on-failure -E "interactive"
```

### Debug Assertion Failed (Array Bounds)

MSVC debug runtime catches array bounds violations. This indicates a bug in test code accessing an empty vector with `[0]`. Build in Release to suppress the dialog, or run with the test runner that catches the assertion.

---

## Next Steps

- [Getting Started](getting_started.md) — write your first rule
- [Core Concepts](concepts.md) — rules, workflows, expressions
- [Extensions](extensions/) — JSON, XML, database persistence
