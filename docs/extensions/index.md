---
layout: default
title: Extensions
nav_order: 4
has_children: true
permalink: /extensions/
---

# Extensions

FastRules core is dependency-light. Persistence and serialization are provided by optional extensions that you link only when needed.

| Extension | Library | Purpose | Dependencies |
|---|---|---|---|
| fastrules-json | `fastrules-json` | Load/save rules from JSON | nlohmann/json |
| fastrules-xml | `fastrules-xml` | Load/save rules from XML | pugixml |
| fastrules-db | `fastrules-db` | Database persistence | SOCI |

## Quick Comparison

| Feature | JSON | XML | DB |
|---|---|---|---|
| Human-readable | Yes | Yes | No |
| Version control friendly | Yes | Yes | No |
| Runtime updates | Regenerate file | Regenerate file | Update in place |
| Concurrent access | File locking | File locking | Transactions |
| Query by rule ID | Manual | Manual | SQL |
| Best for | Config files, deployments | Legacy interchange | Production systems |

## Installation

All extensions are built from the same source tree:

```bash
cmake -B build -S . \
    -DFASTRULES_BUILD_EXTENSIONS=ON \
    -DFASTRULES_BUILD_DB=ON  # optional
```

Then link what you need:

```cmake
target_link_libraries(your_target
    fastrules        # core
    fastrules-json   # JSON support
    fastrules-xml    # XML support
    fastrules-db     # DB support (optional)
)
```

## Child Pages

- [JSON Extension](json.md)
- [XML Extension](xml.md)
- [Database Extension](db.md)
