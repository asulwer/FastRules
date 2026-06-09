// type_registration_macro.hpp
// Optional convenience macros for registerType field/method binding.
// Use if you prefer macro syntax over lambda syntax.
//
// Usage:
//   FASTRULES_REGISTER_TYPE(engine, Customer, name, age);
//   FASTRULES_REGISTER_METHODS(engine, Customer, isPremium, getTier);
//
// Or combined:
//   FASTRULES_REGISTER_TYPE_WITH_METHODS(engine, Customer,
//       (name, age, processed),            // fields
//       (isPremium, getTier)               // methods
//   );

#pragma once

// ============================================================================
// Field binding macro — generates reg.bind() calls
// ============================================================================

#define FASTRULES_BIND_FIELD_1(reg, TypeName, field1) \
    reg.bind(#field1, &TypeName::field1)

#define FASTRULES_BIND_FIELD_2(reg, TypeName, field1, field2) \
    FASTRULES_BIND_FIELD_1(reg, TypeName, field1); \
    reg.bind(#field2, &TypeName::field2)

#define FASTRULES_BIND_FIELD_3(reg, TypeName, field1, field2, field3) \
    FASTRULES_BIND_FIELD_2(reg, TypeName, field1, field2); \
    reg.bind(#field3, &TypeName::field3)

#define FASTRULES_BIND_FIELD_4(reg, TypeName, field1, field2, field3, field4) \
    FASTRULES_BIND_FIELD_3(reg, TypeName, field1, field2, field3); \
    reg.bind(#field4, &TypeName::field4)

#define FASTRULES_BIND_FIELD_5(reg, TypeName, field1, field2, field3, field4, field5) \
    FASTRULES_BIND_FIELD_4(reg, TypeName, field1, field2, field3, field4); \
    reg.bind(#field5, &TypeName::field5)

#define FASTRULES_BIND_FIELD_6(reg, TypeName, field1, field2, field3, field4, field5, field6) \
    FASTRULES_BIND_FIELD_5(reg, TypeName, field1, field2, field3, field4, field5); \
    reg.bind(#field6, &TypeName::field6)

#define FASTRULES_BIND_FIELD_7(reg, TypeName, field1, field2, field3, field4, field5, field6, field7) \
    FASTRULES_BIND_FIELD_6(reg, TypeName, field1, field2, field3, field4, field5, field6); \
    reg.bind(#field7, &TypeName::field7)

#define FASTRULES_BIND_FIELD_8(reg, TypeName, field1, field2, field3, field4, field5, field6, field7, field8) \
    FASTRULES_BIND_FIELD_7(reg, TypeName, field1, field2, field3, field4, field5, field6, field7); \
    reg.bind(#field8, &TypeName::field8)

// ============================================================================
// Method binding macro — generates reg.method() calls
// ============================================================================

#define FASTRULES_BIND_METHOD_1(reg, TypeName, method1) \
    reg.method(#method1, &TypeName::method1)

#define FASTRULES_BIND_METHOD_2(reg, TypeName, method1, method2) \
    FASTRULES_BIND_METHOD_1(reg, TypeName, method1); \
    reg.method(#method2, &TypeName::method2)

#define FASTRULES_BIND_METHOD_3(reg, TypeName, method1, method2, method3) \
    FASTRULES_BIND_METHOD_2(reg, TypeName, method1, method2); \
    reg.method(#method3, &TypeName::method3)

#define FASTRULES_BIND_METHOD_4(reg, TypeName, method1, method2, method3, method4) \
    FASTRULES_BIND_METHOD_3(reg, TypeName, method1, method2, method3); \
    reg.method(#method4, &TypeName::method4)

// ============================================================================
// Dispatch macros (internal use)
// ============================================================================

#define FASTRULES_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

#define FASTRULES_DISPATCH_FIELDS(reg, TypeName, ...) \
    FASTRULES_GET_MACRO(__VA_ARGS__, \
        FASTRULES_BIND_FIELD_8, FASTRULES_BIND_FIELD_7, FASTRULES_BIND_FIELD_6, \
        FASTRULES_BIND_FIELD_5, FASTRULES_BIND_FIELD_4, FASTRULES_BIND_FIELD_3, \
        FASTRULES_BIND_FIELD_2, FASTRULES_BIND_FIELD_1)(reg, TypeName, __VA_ARGS__)

#define FASTRULES_DISPATCH_METHODS(reg, TypeName, ...) \
    FASTRULES_GET_MACRO(__VA_ARGS__, \
        FASTRULES_BIND_METHOD_4, FASTRULES_BIND_METHOD_4, FASTRULES_BIND_METHOD_3, \
        FASTRULES_BIND_METHOD_3, FASTRULES_BIND_METHOD_2, FASTRULES_BIND_METHOD_2, \
        FASTRULES_BIND_METHOD_1, FASTRULES_BIND_METHOD_1)(reg, TypeName, __VA_ARGS__)

// ============================================================================
// Public API macros
// ============================================================================

// Register fields only
// Usage: FASTRULES_REGISTER_TYPE(engine, Customer, name, age, balance)
#define FASTRULES_REGISTER_TYPE(engine, TypeName, ...) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { \
        FASTRULES_DISPATCH_FIELDS(reg, TypeName, __VA_ARGS__); \
    })

// Register methods only
// Usage: FASTRULES_REGISTER_METHODS(engine, Customer, isPremium, getTier)
#define FASTRULES_REGISTER_METHODS(engine, TypeName, ...) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { \
        FASTRULES_DISPATCH_METHODS(reg, TypeName, __VA_ARGS__); \
    })

// Register both fields and methods
// Usage: FASTRULES_REGISTER_TYPE_WITH_METHODS(engine, Customer,
//     (name, age, balance),      // fields — up to 8
//     (isPremium, getTier)       // methods — up to 4
// );
//
// Note: The inner parentheses are required to separate field and method lists.
// This uses the "parenthesized arguments" trick since VA_ARGS can't have two lists.
#define FASTRULES_REGISTER_TYPE_WITH_METHODS(engine, TypeName, fields_tuple, methods_tuple) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { \
        FASTRULES_EXPAND_FIELDS fields_tuple \
        FASTRULES_EXPAND_METHODS methods_tuple \
    })

// Helper: strip parentheses from tuple notation
#define FASTRULES_EXPAND_FIELDS(...) FASTRULES_DISPATCH_FIELDS(reg, TypeName, __VA_ARGS__)
#define FASTRULES_EXPAND_METHODS(...) FASTRULES_DISPATCH_METHODS(reg, TypeName, __VA_ARGS__)
