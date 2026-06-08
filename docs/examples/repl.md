---
layout: default
title: REPL Example
parent: Examples
nav_order: 8
---

# REPL Example

Interactive Lua expression testing via command line.

## Overview

A simple read-eval-print loop for testing FastRules expressions interactively. Type Lua expressions and see results immediately.

## Source

```cpp
#include <fastrules.hpp>
#include <iostream>
#include <string>

int main() {
    fastrules::LuaEngine engine;
    std::string line;

    std::cout << "FastRules REPL — type Lua expressions (exit to quit)\n";
    std::cout << "Examples: 1 + 1, true and false, string.len(\"hello\")\n\n";

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        try {
            auto ref = engine.compileExpression(line, {});
            if (!ref.has_value()) {
                std::cout << "Error: compilation failed\n";
                continue;
            }

            fastrules::RuleContext ctx;
            bool result = engine.evaluateExpression(ref.value(), {}, ctx);
            std::cout << "Result: " << (result ? "true" : "false") << "\n";
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }

    return 0;
}
```

## Building

```bash
cmake -B build -S . -DFASTRULES_BUILD_EXAMPLES=ON
cmake --build build --target repl_example
```

## Usage

```bash
./build/repl_example
> 1 + 1
Result: true
> 10 > 5
Result: true
> true and false
Result: false
> exit
```

## Notes

- Expressions must return a boolean (FastRules convention)
- Use `registerType<T>()` to test with C++ structs
- Actions are not supported in REPL mode (no rule context)
