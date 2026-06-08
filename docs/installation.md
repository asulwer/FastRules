---
layout: default
title: Installation
nav_order: 10
---

# Installation

## Requirements

- CMake 3.28+
- C++23 compiler (Visual Studio 2022, GCC 13+, Clang 17+)
- Git

## Build from Source

### 1. Clone the Repository

```bash
git clone https://github.com/asulwer/fastrules.git
cd fastrules
```

### 2. Configure with CMake

```bash
mkdir build && cd build
cmake ..
```

This downloads dependencies (sol2, nlohmann/json) via CMake FetchContent.

### 3. Build

```bash
cmake --build . --config Release
```

### 4. Run Tests

```bash
ctest --output-on-failure
```

## Integration Options

### CMake FetchContent (Recommended)

Add to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    fastrules
    GIT_REPOSITORY https://github.com/asulwer/fastrules.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(fastrules)

target_link_libraries(your_app PRIVATE fastrules)
```

### Copy Sources

Copy `include/` and `src/` into your project, or build `FastRules` as a static library.

## Optional Features

| CMake Option | Default | Description |
|-------------|---------|-------------|
| `FASTRULES_BUILD_TESTS` | `ON` | Build Catch2 test suite |
| `FASTRULES_BUILD_EXAMPLES` | `ON` | Build example programs |

```bash
cmake -DFASTRULES_BUILD_EXAMPLES=OFF ..
```
