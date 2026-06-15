# FastRules C# Example

This example demonstrates how to use FastRules from C# via P/Invoke with in-memory workflow/rule creation (no JSON required).

## Prerequisites

1. **Build FastRules with C API:**
   ```powershell
   .\build.ps1
   ```
   
   Or manually:
   ```bash
   cmake -B build -S . \
     -DFASTRULES_BUILD_SHARED=ON \
     -DFASTRULES_BUILD_C_API=ON
   cmake --build build --config Release
   ```

2. **.NET 8.0 SDK** (or later)

## Usage

The C API DLL and its dependencies are automatically copied to this directory during the build.

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
   FastRules C++ Library (Core only)
       |
       v
   Lua Engine
```

## Key Features

- **In-memory workflow creation** - No JSON required
- **Type-safe wrapper** around the C API
- **Complex object support** - Pass Customer objects directly
- **Parent-child rules** - Access child results from parent rules
- **IDisposable pattern** for proper resource cleanup

## Example Code

```csharp
using FastRulesExample;

// Create engine
using var engine = new FastRulesEngine();
Console.WriteLine($"Version: {engine.Version}");

// Create workflow in-memory (no JSON)
using var workflow = engine.CreateWorkflow(1, "Customer Validation");

// Add rules programmatically
workflow.AddRule(1, "age >= 18", description: "Age check");
workflow.AddRule(2, "len(name) > 0", description: "Name check");

workflow.Compile();

// Execute
var results = workflow.Execute(new Dictionary<string, object>
{
    ["age"] = 25,
    ["name"] = "Alice"
});

foreach (var result in results)
{
    Console.WriteLine($"{result.RuleName}: {(result.Success ? "PASS" : "FAIL")}");
}
```

## Complex Types (Customer Object)

```csharp
// Register type with engine
var customerType = engine.RegisterType("Customer", "age:int;name:string;balance:double;isActive:bool;tier:string");

// Create Customer object
var customer = new Customer(age: 25, name: "Alice", balance: 100.0, isActive: true);
using var obj = customer.ToFastRulesObject(engine, customerType);

// Execute with complex object
var results = workflow.Execute(new Dictionary<string, object> { ["customer"] = obj });
```

## Troubleshooting

### "Could not load fastrules_c_api.dll"

Ensure the DLL is:
1. Built with C API enabled (`-DFASTRULES_BUILD_C_API=ON`)
2. Copied to the output directory
3. All dependencies (lua.dll, spdlog.dll, fmt.dll) are available

### "Entry point not found"

The C API must be exported with `extern "C"` and proper calling convention:
```cpp
extern "C" __declspec(dllexport) void* fastrules_engine_create();
```
