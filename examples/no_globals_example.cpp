// no_globals_example.cpp
// What the code would look like without globals
// Parameters passed as function arguments instead

#include <fastrules.hpp>
#include <iostream>
#include <vector>

struct Customer {
    std::string name;
    int age = 0;
    bool processed = false;

    bool operator==(const Customer& o) const { return name == o.name && age == o.age; }
    bool operator<=(const Customer& o) const { return age <= o.age; }
    bool operator<(const Customer& o) const { return age < o.age; }
};

// ============================================================================
// CURRENT (with globals):
// ============================================================================
// Expression: "customer.age >= 18"
// Action:     "customer.processed = true"
// 
// Compiled as: return function() return (customer.age >= 18) end
// 
// Parameters pushed as globals:
//   lua_.set("customer", customerPtr)
//
// Problem: Globals leak between executions
// ============================================================================

// ============================================================================
// WITHOUT GLOBALS:
// ============================================================================
// Expression: "customer.age >= 18"
// Action:     "customer.processed = true"
//
// Compiled as: return function(customer) return (customer.age >= 18) end
//
// Parameters passed as arguments:
//   func(customerPtr)
//
// Benefit: No globals, no leakage, cleaner isolation
// ============================================================================

// Hypothetical API (not implemented):
class NoGlobalsEngine {
public:
    // Compile with explicit parameter list
    // The engine parses the expression to extract parameter names
    template<typename... ParamNames>
    int compileExpression(const std::string& expression, ParamNames... names) {
        // Build function signature: function(customer, order, ...)
        std::string sig = buildSignature(names...);
        std::string wrapped = "return function(" + sig + ")\n";
        wrapped += "    return (" + expression + ")\n";
        wrapped += "end\n";
        // ... compile and store ref
        return 0; // ref
    }

    // Execute with arguments in order
    template<typename... Args>
    bool execute(int ref, Args... args) {
        // func(customerPtr, orderPtr, ...)
        // No globals set!
        return true;
    }

private:
    template<typename... Names>
    std::string buildSignature(Names... names) {
        // "customer, order"
        std::string sig;
        // ... concatenate with commas
        return sig;
    }
};

// ============================================================================
// USAGE COMPARISON:
// ============================================================================

void demo_current_api() {
    fastrules::LuaEngine engine;
    
    // Register type
    engine.registerType<Customer>("Customer", [](auto& reg) {
        reg.bind("age", &Customer::age);
        reg.bind("processed", &Customer::processed);
    });

    // Create rule
    auto rule = std::make_shared<fastrules::Rule>();
    rule->id = 1;
    rule->expression = "customer.age >= 18";
    rule->action = "customer.processed = true";

    // Compile
    rule->compile(engine);

    // Execute
    Customer customer{"Alice", 25};
    std::vector<fastrules::RuleParameter> params;
    params.emplace_back("customer", &customer);

    auto context = fastrules::RuleContext();
    auto result = rule->execute(engine, context, params);
    // Internally: sets global "customer", calls function(), clears globals
}

// Hypothetical no-globals usage:
void demo_no_globals_api() {
    // NoGlobalsEngine engine;
    // engine.registerType<Customer>("Customer", ...);
    //
    // // Compile WITH parameter names
    // int ref = engine.compileExpression(
    //     "customer.age >= 18",
    //     "customer"  // explicit param name
    // );
    //
    // // Execute with arguments directly
    // Customer customer{"Alice", 25};
    // auto result = engine.execute(ref, &customer);  // Pass as arg!
    // No globals touched
}

int main() {
    std::cout << "=== Current API (with globals) ===" << std::endl;
    demo_current_api();
    std::cout << "Works fine, but uses globals internally" << std::endl;

    std::cout << "\n=== No-Globals API (hypothetical) ===" << std::endl;
    demo_no_globals_api();
    std::cout << "Would pass parameters as function arguments" << std::endl;

    std::cout << "\nKey differences:" << std::endl;
    std::cout << "- Current: compileExpression(expr, {}) - ignores param names" << std::endl;
    std::cout << "- No-globals: compileExpression(expr, {\"customer\"}) - explicit params" << std::endl;
    std::cout << "- Current: execute(ref, params) - sets globals" << std::endl;
    std::cout << "- No-globals: execute(ref, customerPtr) - passes args" << std::endl;

    return 0;
}
