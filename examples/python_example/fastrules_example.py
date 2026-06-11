#!/usr/bin/env python3
"""
FastRules Python Example

This example demonstrates how to use FastRules from Python via ctypes.
For production use, consider creating a proper Python extension module
using pybind11 or Cython for better performance and type safety.

Requirements:
- Python 3.7+
- fastrules shared library (fastrules.dll on Windows, libfastrules.so on Linux)

Build the shared library:
    cmake -B build -DFASTRULES_BUILD_SHARED=ON
    cmake --build build
"""

import ctypes
import os
import sys
from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from enum import IntEnum


# Load the FastRules shared library
def load_fastrules_library():
    """Load the FastRules shared library based on platform."""
    if sys.platform == "win32":
        lib_name = "fastrules.dll"
    elif sys.platform == "linux":
        lib_name = "libfastrules.so"
    elif sys.platform == "darwin":
        lib_name = "libfastrules.dylib"
    else:
        raise RuntimeError(f"Unsupported platform: {sys.platform}")
    
    # Try to find library in common locations
    search_paths = [
        os.path.join(os.path.dirname(__file__), "..", "..", "build", "Release"),
        os.path.join(os.path.dirname(__file__), "..", "..", "build"),
        os.path.join(os.path.dirname(__file__), "..", "..", "build_vs", "Release"),
        os.path.dirname(__file__),
    ]
    
    for path in search_paths:
        full_path = os.path.join(path, lib_name)
        if os.path.exists(full_path):
            return ctypes.CDLL(full_path)
    
    # Try system paths
    try:
        return ctypes.CDLL(lib_name)
    except OSError:
        raise RuntimeError(
            f"Could not find {lib_name}. "
            "Please build FastRules with -DFASTRULES_BUILD_SHARED=ON "
            "or ensure the library is in your PATH/LD_LIBRARY_PATH."
        )


@dataclass
class RuleResult:
    """Represents the result of rule execution."""
    rule_id: int
    success: bool
    error_message: Optional[str] = None


class FastRulesEngine:
    """Python wrapper for FastRules C++ library."""
    
    def __init__(self):
        """Initialize the FastRules engine."""
        self._lib = load_fastrules_library()
        self._setup_function_signatures()
        self._engine = None
        self._create_engine()
    
    def _setup_function_signatures(self):
        """Setup ctypes function signatures for C API functions."""
        # Note: This assumes a C API wrapper exists. If not, you'll need to
        # create one or use a different approach (e.g., pybind11, Cython)
        
        # Example C API function signatures:
        # extern "C" {
        #     void* fastrules_engine_create();
        #     void fastrules_engine_destroy(void* engine);
        #     int fastrules_execute_workflow(void* engine, const char* json, ...);
        # }
        
        # For now, this is a placeholder that demonstrates the approach
        pass
    
    def _create_engine(self):
        """Create the underlying LuaEngine instance."""
        # This would call the C API to create an engine
        # self._engine = self._lib.fastrules_engine_create()
        pass
    
    def __del__(self):
        """Cleanup the engine."""
        if self._engine:
            # self._lib.fastrules_engine_destroy(self._engine)
            pass
    
    def execute_rules(self, rules_json: str, context: Dict[str, Any]) -> List[RuleResult]:
        """
        Execute rules defined in JSON format.
        
        Args:
            rules_json: JSON string defining the workflow and rules
            context: Dictionary of parameters to pass to the rules
        
        Returns:
            List of RuleResult objects
        """
        # This would serialize the context to JSON and call the C API
        # For demonstration, we'll show how it would work:
        
        # Serialize context to JSON
        import json
        context_json = json.dumps(context)
        
        # Call C API (placeholder)
        # result_ptr = self._lib.fastrules_execute_workflow(
        #     self._engine,
        #     rules_json.encode('utf-8'),
        #     context_json.encode('utf-8')
        # )
        
        # Parse results (placeholder)
        results = []
        # ... parse C results into Python objects
        
        return results


def example_basic_usage():
    """Example: Basic rule execution."""
    print("=" * 60)
    print("FastRules Python Example - Basic Usage")
    print("=" * 60)
    
    # Define rules in JSON format (using the JSON extension)
    workflow_json = """
    {
        "id": 1,
        "description": "Customer Validation",
        "rules": [
            {
                "id": 1,
                "description": "Age check",
                "expression": "age >= 18"
            },
            {
                "id": 2,
                "description": "Name check",
                "expression": "name ~= '^[A-Za-z]+$'"
            }
        ]
    }
    """
    
    try:
        engine = FastRulesEngine()
        
        # Test with valid customer
        print("\n--- Testing Valid Customer (Alice, age 25) ---")
        context = {"age": 25, "name": "Alice"}
        results = engine.execute_rules(workflow_json, context)
        for result in results:
            status = "PASS" if result.success else "FAIL"
            print(f"  Rule {result.rule_id}: {status}")
        
        # Test with invalid customer (minor)
        print("\n--- Testing Minor Customer (Bob, age 15) ---")
        context = {"age": 15, "name": "Bob"}
        results = engine.execute_rules(workflow_json, context)
        for result in results:
            status = "PASS" if result.success else "FAIL"
            print(f"  Rule {result.rule_id}: {status}")
        
        # Test with invalid name
        print("\n--- Testing Invalid Name (Charlie123) ---")
        context = {"age": 30, "name": "Charlie123"}
        results = engine.execute_rules(workflow_json, context)
        for result in results:
            status = "PASS" if result.success else "FAIL"
            print(f"  Rule {result.rule_id}: {status}")
        
    except Exception as e:
        print(f"Error: {e}")
        print("\nNote: This example requires:")
        print("  1. A C API wrapper for FastRules (see fastrules_c_api.h)")
        print("  2. The FastRules shared library built with -DFASTRULES_BUILD_SHARED=ON")


def example_workflow_with_actions():
    """Example: Workflow with actions."""
    print("\n" + "=" * 60)
    print("FastRules Python Example - Actions")
    print("=" * 60)
    
    workflow_json = """
    {
        "id": 2,
        "description": "Order Processing",
        "rules": [
            {
                "id": 1,
                "description": "Check minimum order",
                "expression": "total >= 10.00",
                "action": "applyDiscount(0.10)"
            },
            {
                "id": 2,
                "description": "Check VIP status",
                "expression": "isVip == true",
                "action": "applyDiscount(0.20)"
            }
        ]
    }
    """
    
    try:
        engine = FastRulesEngine()
        
        print("\n--- Processing Standard Order ($100, non-VIP) ---")
        context = {"total": 100.00, "isVip": False}
        results = engine.execute_rules(workflow_json, context)
        for result in results:
            status = "PASS" if result.success else "FAIL"
            print(f"  Rule {result.rule_id}: {status}")
        
        print("\n--- Processing VIP Order ($100, VIP) ---")
        context = {"total": 100.00, "isVip": True}
        results = engine.execute_rules(workflow_json, context)
        for result in results:
            status = "PASS" if result.success else "FAIL"
            print(f"  Rule {result.rule_id}: {status}")
        
    except Exception as e:
        print(f"Error: {e}")


def example_parallel_execution():
    """Example: Parallel rule execution."""
    print("\n" + "=" * 60)
    print("FastRules Python Example - Parallel Execution")
    print("=" * 60)
    
    # This would require exposing executeParallel via C API
    print("\nNote: Parallel execution requires additional C API bindings.")
    print("See AsyncWorkflow documentation for details.")


def main():
    """Run all examples."""
    print("FastRules Python Examples")
    print("=" * 60)
    print()
    print("Note: These examples demonstrate the Python API structure.")
    print("To run them, you need:")
    print("  1. A C API wrapper for FastRules (fastrules_c_api.h/cpp)")
    print("  2. Python bindings (ctypes, pybind11, or Cython)")
    print("  3. FastRules built as a shared library")
    print()
    
    example_basic_usage()
    example_workflow_with_actions()
    example_parallel_execution()
    
    print("\n" + "=" * 60)
    print("For production use, consider:")
    print("  - Using pybind11 for better C++ integration")
    print("  - Creating a proper Python package with setup.py/pyproject.toml")
    print("  - Adding type hints and documentation")
    print("=" * 60)


if __name__ == "__main__":
    main()
