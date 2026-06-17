# FastRules Extensions

FastRules uses a **pluggable extension architecture**. The core library (`fastrules`) has zero JSON/XML/DB dependencies. Add only the extensions you need.

## Available Extensions

| Extension | Purpose | Dependencies | Target |
|-----------|---------|-------------|--------|
| `fastrules-json` | JSON persistence | nlohmann/json | `fastrules::json` |
| `fastrules-xml` | XML persistence | pugixml | `fastrules::xml` |
| `fastrules-db` | Database persistence | SOCI | `fastrules::db` |

## Architecture

```
extensions/
├── json/              # fastrules-json target
│   ├── src/
│   │   ├── json_repository.cpp
│   │   └── json_loader.cpp
│   ├── include/fastrules/
│   │   └── json_loader.hpp
│   ├── tests/
│   └── CMakeLists.txt
├── xml/               # fastrules-xml target
│   ├── src/
│   │   └── xml_loader.cpp
│   ├── include/fastrules/
│   │   └── xml_loader.hpp
│   ├── tests/
│   └── CMakeLists.txt
└── db/                # fastrules-db target (requires SOCI)
    ├── src/
    │   └── db_repository.cpp
    ├── include/fastrules/
    │   └── db_repository.hpp
    ├── examples/
    └── CMakeLists.txt
```

## Building Extensions

### CMake

```bash
# Build with all extensions
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON

# Build with specific extensions only
cmake -B build -S . -DFASTRULES_BUILD_EXTENSIONS=ON -DFASTRULES_BUILD_DB=OFF
```

### vcpkg

```bash
vcpkg install fastrules[json,xml]
```

### Conan

```bash
conan install fastrules/0.1.0 -o with_xml=True -o with_db=False
```

## Linking Your Application

```cmake
target_link_libraries(myapp
    PRIVATE
        fastrules::fastrules        # Core
        fastrules::fastrules-json   # JSON support
        fastrules::fastrules-xml    # XML support
)
```

## Creating a New Extension

To create a new persistence extension (e.g., YAML, Protobuf, etc.):

1. **Create directory structure:**
   ```
   extensions/myext/
   ├── src/
   │   └── myext_loader.cpp
   ├── include/fastrules/
   │   └── myext_loader.hpp
   ├── tests/
   │   └── test_myext_loader.cpp
   └── CMakeLists.txt
   ```

2. **Write CMakeLists.txt:**
   ```cmake
   add_library(fastrules-myext
       src/myext_loader.cpp
   )
   
   target_include_directories(fastrules-myext
       PUBLIC
           ${CMAKE_CURRENT_SOURCE_DIR}/include
   )
   
   target_link_libraries(fastrules-myext
       PUBLIC
           fastrules
           myext_library  # Your dependency
   )
   ```

3. **Add to main CMakeLists.txt:**
   ```cmake
   # In extensions/CMakeLists.txt
   add_subdirectory(myext)
   ```

4. **Write tests** using doctest

## Extension Guidelines

- **Header-only extensions preferred** when possible (simpler deployment)
- **No core modifications** — extensions only add to the API, never change it
- **Version lockstep** — extension version matches core version
- **Optional features** — `find_package` with fallback to `FetchContent` for deps
