# FastRules C# Example

This example demonstrates how to use FastRules from C# via P/Invoke.

## Prerequisites

1. **Build FastRules as a shared library:**
   ```bash
   cmake -B build -S . \
     -DFASTRULES_BUILD_SHARED=ON \
     -DFASTRULES_BUILD_EXTENSIONS=ON
   cmake --build build --config Release
   ```

2. **.NET 8.0 SDK** (or later)

3. **Copy the FastRules DLL** to the example directory:
   ```powershell
   # Windows
   copy ..\..\build\Release\fastrules.dll .
   copy ..\..\build\Release\fastrules-json.dll .
   ```

## Usage

```powershell
# Run the example
dotnet run

# Or build and run separately
dotnet build
dotnet run --project FastRulesExample.csproj
```

## Architecture

```
FastRulesExample.cs
       |
       v
   [P/Invoke]
       |
       v
fastrules_c_api.h / fastrules_c_api.cpp (C wrapper)
       |
       v
   FastRules C++ Library
       |
       v
   Lua Engine
```

## Key Features

- **Type-safe wrapper** around the C API
- **JSON serialization** for parameters and results
- **IDisposable pattern** for proper resource cleanup
- **Async support** can be added for ExecuteAsync

## For Production Use

Consider these improvements:

1. **NuGet Package**:
   - Create a proper NuGet package
   - Include native libraries for all platforms
   - MSBuild targets for automatic copying

2. **Source Generators**:
   - Generate P/Invoke signatures automatically
   - JSON contract generation

3. **Async API**:
   - Add async/await support
   - Task-based execution

4. **Span<T> and Memory<T>**:
   - Zero-copy where possible
   - Better performance

## Example Code

```csharp
using FastRulesExample;

// Create engine
using var engine = new FastRulesEngine();
Console.WriteLine($"Version: {engine.Version}");

// Load workflow
var workflowJson = @"{
    ""id"": 1,
    ""rules"": [
        { ""id"": 1, ""expression"": ""age >= 18"" }
    ]
}";

using var workflow = engine.LoadWorkflow(workflowJson);
workflow.Compile();

// Execute
var results = workflow.Execute(new Dictionary<string, object>
{
    ["age"] = 25
});

foreach (var result in results)
{
    Console.WriteLine($"Rule {result.RuleId}: {(result.Success ? "PASS" : "FAIL")}");
}
```

## Troubleshooting

### "Could not load fastrules.dll"

Ensure the DLL is:
1. Built as a shared library (`-DFASTRULES_BUILD_SHARED=ON`)
2. Copied to the output directory
3. All dependencies (Lua, fmt, spdlog) are available

### "Entry point not found"

The C API must be exported with `extern "C"` and proper calling convention:
```cpp
extern "C" __declspec(dllexport) void* fastrules_engine_create();
```
