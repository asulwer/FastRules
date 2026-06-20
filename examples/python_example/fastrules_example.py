#!/usr/bin/env python3
"""
FastRules Python Example

This example demonstrates how to use FastRules from Python via ctypes
with in-memory workflow/rule creation (no JSON required).

Requirements:
- Python 3.7+
- fastrules shared library (fastrules.dll on Windows, libfastrules.so on Linux)

Build with:
    cmake -B build -DFASTRULES_BUILD_SHARED=ON
    Then copy fastrules.dll (or libfastrules.so) and dependencies to this directory.
"""

import ctypes
import os
import sys
from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from enum import IntEnum


class ErrorCode(IntEnum):
    """FastRules error codes."""
    OK = 0
    NULL_PTR = -1
    COMPILATION_FAILED = -3
    EXECUTION_FAILED = -4
    MEMORY = -5
    UNKNOWN = -99


@dataclass
class RuleResult:
    """Represents the result of rule execution."""
    rule_id: int
    rule_name: str
    success: bool
    error_message: Optional[str] = None


class FastRulesEngine:
    """Python wrapper for FastRules C++ library using ctypes."""
    
    def __init__(self):
        """Initialize the FastRules engine."""
        self._lib = self._load_library()
        self._setup_function_signatures()
        
        # Create engine
        self._engine = self._lib.fastrules_engine_create()
        if not self._engine:
            raise RuntimeError("Failed to create FastRules engine")
    
    def _load_library(self) -> ctypes.CDLL:
        """Load the FastRules C API shared library."""
        if sys.platform == "win32":
            lib_name = "fastrules.dll"
        elif sys.platform == "linux":
            lib_name = "libfastrules.so"
        elif sys.platform == "darwin":
            lib_name = "libfastrules.dylib"
        else:
            raise RuntimeError(f"Unsupported platform: {sys.platform}")
        
        # Try to find library in the current directory first
        script_dir = os.path.dirname(os.path.abspath(__file__))
        search_paths = [
            script_dir,
            os.path.join(script_dir, "..", "..", "build_vs", "Release"),
            os.path.join(script_dir, "..", "..", "build_vs", "Debug"),
        ]
        
        for path in search_paths:
            full_path = os.path.join(path, lib_name)
            if os.path.exists(full_path):
                return ctypes.CDLL(full_path)
        
        # Try system paths
        try:
            return ctypes.CDLL(lib_name)
        except OSError as e:
            raise RuntimeError(
                f"Could not find {lib_name}. "
                f"Please build FastRules C API first. "
                f"Error: {e}"
            )
    
    def _setup_function_signatures(self):
        """Setup ctypes function signatures for C API functions."""
        lib = self._lib
        
        # Engine functions
        lib.fastrules_engine_create.restype = ctypes.c_void_p
        lib.fastrules_engine_create.argtypes = []
        
        lib.fastrules_engine_destroy.restype = None
        lib.fastrules_engine_destroy.argtypes = [ctypes.c_void_p]
        
        lib.fastrules_engine_get_last_error.restype = ctypes.c_char_p
        lib.fastrules_engine_get_last_error.argtypes = [ctypes.c_void_p]
        
        # Workflow creation (in-memory)
        lib.fastrules_workflow_create.restype = ctypes.c_void_p
        lib.fastrules_workflow_create.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
        
        lib.fastrules_workflow_add_rule.restype = ctypes.c_int
        lib.fastrules_workflow_add_rule.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int,
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_bool
        ]
        
        lib.fastrules_workflow_set_rule_priority.restype = ctypes.c_int
        lib.fastrules_workflow_set_rule_priority.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int
        ]
        
        lib.fastrules_workflow_destroy.restype = None
        lib.fastrules_workflow_destroy.argtypes = [ctypes.c_void_p]
        
        lib.fastrules_workflow_compile.restype = ctypes.c_int
        lib.fastrules_workflow_compile.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        
        lib.fastrules_workflow_execute.restype = ctypes.c_int
        lib.fastrules_workflow_execute.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)
        ]
        
        lib.fastrules_free.restype = None
        lib.fastrules_free.argtypes = [ctypes.c_void_p]
        
        lib.fastrules_get_version.restype = ctypes.c_char_p
        lib.fastrules_get_version.argtypes = []
        
        # Complex Object Support
        lib.fastrules_register_type.restype = ctypes.c_void_p
        lib.fastrules_register_type.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
        
        lib.fastrules_object_create.restype = ctypes.c_void_p
        lib.fastrules_object_create.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        
        lib.fastrules_object_set_field.restype = ctypes.c_int
        lib.fastrules_object_set_field.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
        
        lib.fastrules_object_destroy.restype = None
        lib.fastrules_object_destroy.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        
        lib.fastrules_add_object_param.restype = ctypes.c_int
        lib.fastrules_add_object_param.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, 
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)
        ]
    
    def __del__(self):
        """Cleanup the engine."""
        if hasattr(self, '_engine') and self._engine:
            self._lib.fastrules_engine_destroy(self._engine)
            self._engine = None
    
    def get_last_error(self) -> str:
        """Get the last error message."""
        msg = self._lib.fastrules_engine_get_last_error(self._engine)
        return msg.decode('utf-8') if msg else "Unknown error"
    
    def get_version(self) -> str:
        """Get FastRules version."""
        return self._lib.fastrules_get_version().decode('utf-8')
    
    def register_type(self, type_name: str, fields: str) -> 'ComplexType':
        """Register a complex type with the engine.
        
        Fields format: "field1:type1;field2:type2"
        Supported types: int, double, bool, string
        """
        name = type_name.encode('utf-8')
        fields_b = fields.encode('utf-8')
        
        type_ptr = self._lib.fastrules_register_type(self._engine, name, fields_b)
        if not type_ptr:
            raise RuntimeError(f"Failed to register type: {self.get_last_error()}")
        
        return ComplexType(self, type_ptr, type_name)
    
    def create_object(self, type: 'ComplexType') -> 'ComplexObject':
        """Create an object instance of a registered type."""
        obj_ptr = self._lib.fastrules_object_create(self._engine, type.handle)
        if not obj_ptr:
            raise RuntimeError(f"Failed to create object: {self.get_last_error()}")
        
        return ComplexObject(self, obj_ptr, type)
    
    def destroy_object(self, obj: 'ComplexObject'):
        """Destroy an object instance."""
        if obj and obj.handle:
            self._lib.fastrules_object_destroy(self._engine, obj.handle)

    def create_workflow(self, workflow_id: int, description: str = ""):
        """Create a workflow in-memory."""
        desc = description.encode('utf-8') if description else None
        workflow = self._lib.fastrules_workflow_create(self._engine, workflow_id, desc)
        if not workflow:
            raise RuntimeError(f"Failed to create workflow: {self.get_last_error()}")
        return Workflow(self, workflow)


class ComplexType:
    """Represents a registered complex type."""
    
    def __init__(self, engine: FastRulesEngine, handle, name: str):
        self._engine = engine
        self.handle = handle
        self.name = name


class ComplexObject:
    """Represents an instance of a complex type."""
    
    def __init__(self, engine: FastRulesEngine, handle, type: ComplexType):
        self._engine = engine
        self.handle = handle
        self.type = type
    
    def set_field(self, field_name: str, value):
        """Set a field value on the object."""
        field_b = field_name.encode('utf-8')
        
        # Convert value to string
        if isinstance(value, bool):
            value_str = "true" if value else "false"
        else:
            value_str = str(value)
        value_b = value_str.encode('utf-8')
        
        result = self._engine._lib.fastrules_object_set_field(
            self._engine._engine, self.handle, field_b, value_b
        )
        if result != ErrorCode.OK:
            raise RuntimeError(f"Failed to set field: {self._engine.get_last_error()}")
    
    def to_parameter_string(self, param_name: str) -> str:
        """Convert this object to a parameter string for workflow execution."""
        name_b = param_name.encode('utf-8')
        out_params = ctypes.c_char_p()
        
        result = self._engine._lib.fastrules_add_object_param(
            self._engine._engine, None, name_b, self.handle, ctypes.byref(out_params)
        )
        if result != ErrorCode.OK:
            raise RuntimeError(f"Failed to convert to parameter: {self._engine.get_last_error()}")
        
        try:
            return out_params.value.decode('utf-8')
        finally:
            self._engine._lib.fastrules_free(out_params)
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self._engine.destroy_object(self)


class Workflow:
    """Represents a FastRules workflow."""
    
    def __init__(self, engine: FastRulesEngine, workflow_ptr):
        self._engine = engine
        self._workflow = workflow_ptr
    
    def add_rule(self, rule_id: int, expression: str, name: str = None, action: str = None, 
                 description: str = None, is_active: bool = True):
        """Add a rule to the workflow."""
        expr = expression.encode('utf-8')
        name_b = name.encode('utf-8') if name else None
        act = action.encode('utf-8') if action else None
        desc = description.encode('utf-8') if description else None
        
        result = self._engine._lib.fastrules_workflow_add_rule(
            self._engine._engine, self._workflow, rule_id, name_b, expr, act, desc, is_active
        )
        if result != ErrorCode.OK:
            raise RuntimeError(f"Failed to add rule: {self._engine.get_last_error()}")
    
    def set_rule_priority(self, rule_id: int, priority: int):
        """Set rule priority."""
        result = self._engine._lib.fastrules_workflow_set_rule_priority(
            self._engine._engine, self._workflow, rule_id, priority
        )
        if result != ErrorCode.OK:
            raise RuntimeError(f"Failed to set priority: {self._engine.get_last_error()}")
    
    def compile(self):
        """Compile the workflow."""
        result = self._engine._lib.fastrules_workflow_compile(
            self._engine._engine, self._workflow
        )
        if result != ErrorCode.OK:
            raise RuntimeError(f"Failed to compile workflow: {self._engine.get_last_error()}")
    
    def execute(self, parameters: Dict[str, Any]) -> List[RuleResult]:
        """Execute the workflow with parameters.
        
        Parameters format: "key=value;key2=value2"
        Supported types: int, float, bool, str, ComplexObject
        """
        # Convert parameters to "key=value;key2=value2" format
        parts = []
        for key, value in parameters.items():
            if isinstance(value, ComplexObject):
                # Complex object - use the parameter string from the object
                param_str = value.to_parameter_string(key)
                parts.append(param_str)
            elif isinstance(value, bool):
                val_str = "true" if value else "false"
                parts.append(f"{key}={val_str}")
            else:
                val_str = str(value)
                parts.append(f"{key}={val_str}")
        params_str = ";".join(parts).encode('utf-8')
        
        results_ptr = ctypes.c_char_p()
        
        result = self._engine._lib.fastrules_workflow_execute(
            self._engine._engine, self._workflow, params_str,
            ctypes.byref(results_ptr)
        )
        
        if result != ErrorCode.OK:
            raise RuntimeError(f"Execution failed: {self._engine.get_last_error()}")
        
        try:
            result_str = results_ptr.value.decode('utf-8')
            return self._parse_results(result_str)
        finally:
            self._engine._lib.fastrules_free(results_ptr)
    
    def _parse_results(self, result_str: str) -> List[RuleResult]:
        """Parse result string (format: "id:name:success:error;id:name:success:error")"""
        results = []
        if not result_str:
            return results
        
        for part in result_str.split(';'):
            if not part.strip():
                continue
            fields = part.split(':')
            # Format: id:name:success:error
            if len(fields) >= 3:
                try:
                    rule_id = int(fields[0])
                    rule_name = fields[1]
                    success = fields[2] == '1'
                    error = fields[3] if len(fields) > 3 else None
                    results.append(RuleResult(
                        rule_id=rule_id,
                        rule_name=rule_name,
                        success=success,
                        error_message=error
                    ))
                except (ValueError, IndexError):
                    pass  # Skip malformed entries
        
        return results
    
    def __del__(self):
        """Cleanup the workflow."""
        if hasattr(self, '_workflow') and self._workflow:
            self._engine._lib.fastrules_workflow_destroy(self._workflow)
            self._workflow = None


class Customer:
    """Customer class demonstrating complex type usage."""
    
    def __init__(self, age: int, name: str, balance: float, is_active: bool = True, tier: str = "standard"):
        self.age = age
        self.name = name
        self.balance = balance
        self.is_active = is_active
        self.tier = tier
    
    def to_parameters(self, prefix: str = "customer") -> Dict[str, Any]:
        """Convert Customer to parameter dictionary for FastRules.
        
        This converts the object to flattened parameters.
        For true ComplexObject usage, use to_fastrules_object().
        """
        return {
            f"{prefix}.age": self.age,
            f"{prefix}.name": self.name,
            f"{prefix}.balance": self.balance,
            f"{prefix}.is_active": self.is_active,
            f"{prefix}.tier": self.tier
        }
    
    def to_fastrules_object(self, engine: FastRulesEngine, customer_type: ComplexType) -> ComplexObject:
        """Convert Customer to a FastRules ComplexObject."""
        obj = engine.create_object(customer_type)
        obj.set_field("age", self.age)
        obj.set_field("name", self.name)
        obj.set_field("balance", self.balance)
        obj.set_field("is_active", self.is_active)
        obj.set_field("tier", self.tier)
        return obj


def example_basic_usage():
    """Example: Basic rule execution using in-memory workflow creation."""
    print("=" * 60)
    print("FastRules Python Example - Basic Usage (In-Memory)")
    print("=" * 60)
    
    try:
        engine = FastRulesEngine()
        print(f"FastRules Version: {engine.get_version()}\n")
        
        # Create workflow in-memory (no JSON)
        workflow = engine.create_workflow(1, "Customer Validation")
        
        # Add rules programmatically
        workflow.add_rule(1, "age >= 18", description="Age check")
        workflow.add_rule(2, "len(name) > 0", description="Name check")
        
        workflow.compile()
        
        # Test with valid customer
        print("--- Testing Valid Customer (Alice, age 25) ---")
        results = workflow.execute({"age": 25, "name": "Alice"})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
        # Test with invalid customer (minor)
        print("\n--- Testing Minor Customer (Bob, age 15) ---")
        results = workflow.execute({"age": 15, "name": "Bob"})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
        # Test with empty name
        print("\n--- Testing Empty Name ---")
        results = workflow.execute({"age": 30, "name": ""})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()


def example_order_processing():
    """Example: Order processing with actions."""
    print("\n" + "=" * 60)
    print("FastRules Python Example - Order Processing")
    print("=" * 60)
    
    try:
        engine = FastRulesEngine()
        
        # Create workflow in-memory
        workflow = engine.create_workflow(2, "Order Processing")
        
        # Add rules
        workflow.add_rule(1, "order_total >= 10.00", 
                         description="Minimum order")
        workflow.add_rule(2, "is_vip == true", 
                         description="VIP bonus")
        
        workflow.compile()
        
        print("\n--- Processing Standard Order ($100, non-VIP) ---")
        results = workflow.execute({"order_total": 100.00, "is_vip": False})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
        print("\n--- Processing VIP Order ($100, VIP) ---")
        results = workflow.execute({"order_total": 100.00, "is_vip": True})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()


def example_parent_child():
    """Example: Parent-child rules with context."""
    print("\n" + "=" * 60)
    print("FastRules Python Example - Parent-Child Rules")
    print("=" * 60)
    
    try:
        engine = FastRulesEngine()
        
        # Create workflow with parent-child hierarchy
        workflow = engine.create_workflow(3, "Parent-Child Example")
        
        # Add child rules
        workflow.add_rule(1, "age >= 18", name="age-check",
                         description="Age validation")
        workflow.add_rule(2, "len(name) > 0", name="name-check",
                         description="Name validation")
        
        # Add parent rule referencing children by NAME (not ID)
        workflow.add_rule(3, 'context.getResult("age-check").success == true and context.getResult("name-check").success == true',
                         name="parent-validation",
                         description="Parent validates children")
        
        workflow.compile()
        
        print("\n--- Testing Valid Customer (Alice, age 25) ---")
        results = workflow.execute({"age": 25, "name": "Alice"})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
        print("\n--- Testing Minor (Bob, age 15) ---")
        results = workflow.execute({"age": 15, "name": "Bob"})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()


def example_complex_types():
    """Example: Complex types using Customer class."""
    print("\n" + "=" * 60)
    print("FastRules Python Example - Complex Types (Customer)")
    print("=" * 60)
    
    try:
        engine = FastRulesEngine()
        
        # Register Customer type with the engine
        customer_type = engine.register_type("Customer", "age:int;name:string;balance:double;is_active:bool;tier:string")
        print("Registered Customer type with fields: age, name, balance, is_active, tier")
        
        # Create workflow with customer validation rules
        workflow = engine.create_workflow(4, "Customer Validation")
        
        # Add rules that access customer fields
        workflow.add_rule(1, "customer.age >= 18", name="age-validation",
                         description="Customer must be adult")
        workflow.add_rule(2, "len(customer.name) > 0", name="name-validation",
                         description="Customer must have name")
        workflow.add_rule(3, "customer.balance >= 0", name="balance-check",
                         description="Balance must be positive")
        workflow.add_rule(4, "customer.is_active == true", name="active-check",
                         description="Customer must be active")
        
        # Add parent rule that checks all validations
        workflow.add_rule(5, 
            'context.getResult("age-validation").success == true and '
            'context.getResult("name-validation").success == true and '
            'context.getResult("balance-check").success == true and '
            'context.getResult("active-check").success == true',
            name="customer-approved",
            description="Customer passes all validations")
        
        workflow.compile()
        
        # Test: Valid customer using ComplexObject
        print("\n--- Test: Valid Customer (Alice, 25, $100, Active) ---")
        customer1 = Customer(age=25, name="Alice", balance=100.0, is_active=True, tier="gold")
        obj1 = customer1.to_fastrules_object(engine, customer_type)
        results = workflow.execute({"customer": obj1})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        engine.destroy_object(obj1)
        
        # Test: Minor customer
        print("\n--- Test: Minor Customer (Bob, 15, $50) ---")
        customer2 = Customer(age=15, name="Bob", balance=50.0)
        obj2 = customer2.to_fastrules_object(engine, customer_type)
        results = workflow.execute({"customer": obj2})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        engine.destroy_object(obj2)
        
        # Test: Inactive customer
        print("\n--- Test: Inactive Customer (Charlie, 30, $200, Inactive) ---")
        customer3 = Customer(age=30, name="Charlie", balance=200.0, is_active=False)
        obj3 = customer3.to_fastrules_object(engine, customer_type)
        results = workflow.execute({"customer": obj3})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        engine.destroy_object(obj3)
        
        # Test: Negative balance
        print("\n--- Test: Negative Balance (Dave, 40, -$50) ---")
        customer4 = Customer(age=40, name="Dave", balance=-50.0)
        obj4 = customer4.to_fastrules_object(engine, customer_type)
        results = workflow.execute({"customer": obj4})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        engine.destroy_object(obj4)
        
        # Test: Empty name
        print("\n--- Test: Empty Name (Eve, 35, $500, No name) ---")
        customer5 = Customer(age=35, name="", balance=500.0)
        obj5 = customer5.to_fastrules_object(engine, customer_type)
        results = workflow.execute({"customer": obj5})
        for result in results:
            status = "PASS" if result.success else "FAIL"
            name = result.rule_name if result.rule_name else "(unnamed)"
            print(f"  {name}: {status}")
        engine.destroy_object(obj5)
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()


def main():
    """Run all examples."""
    print("FastRules Python Examples")
    print("=" * 60)
    print("Using in-memory workflow/rule creation (no JSON required)")
    print()
    
    example_basic_usage()
    example_order_processing()
    example_parent_child()
    example_complex_types()
    
    print("\n" + "=" * 60)
    print("Examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
