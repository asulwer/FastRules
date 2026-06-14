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
// Field & method binding primitives
// ============================================================================

#define FASTRULES_BIND_FIELD(reg_, TypeName_, fieldName_) \
    reg_.bind(#fieldName_, &TypeName_::fieldName_);

#define FASTRULES_BIND_METHOD(reg_, TypeName_, methodName_) \
    reg_.method(#methodName_, &TypeName_::methodName_);

// ============================================================================
// Explicit dispatch macros (1-8 fields, 1-4 methods)
// These use MSVC-compatible expansion without __VA_ARGS__ counting tricks.
// ============================================================================

// --- Fields ---
#define FASTRULES_DISPATCH_FIELDS_1(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1)

#define FASTRULES_DISPATCH_FIELDS_2(reg, TypeName, _1, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2)

#define FASTRULES_DISPATCH_FIELDS_3(reg, TypeName, _1, _2, _3) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _3)

#define FASTRULES_DISPATCH_FIELDS_4(reg, TypeName, _1, _2, _3, _4) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _3) \
    FASTRULES_BIND_FIELD(reg, TypeName, _4)

#define FASTRULES_DISPATCH_FIELDS_5(reg, TypeName, _1, _2, _3, _4, _5) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _3) \
    FASTRULES_BIND_FIELD(reg, TypeName, _4) \
    FASTRULES_BIND_FIELD(reg, TypeName, _5)

#define FASTRULES_DISPATCH_FIELDS_6(reg, TypeName, _1, _2, _3, _4, _5, _6) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _3) \
    FASTRULES_BIND_FIELD(reg, TypeName, _4) \
    FASTRULES_BIND_FIELD(reg, TypeName, _5) \
    FASTRULES_BIND_FIELD(reg, TypeName, _6)

#define FASTRULES_DISPATCH_FIELDS_7(reg, TypeName, _1, _2, _3, _4, _5, _6, _7) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _3) \
    FASTRULES_BIND_FIELD(reg, TypeName, _4) \
    FASTRULES_BIND_FIELD(reg, TypeName, _5) \
    FASTRULES_BIND_FIELD(reg, TypeName, _6) \
    FASTRULES_BIND_FIELD(reg, TypeName, _7)

#define FASTRULES_DISPATCH_FIELDS_8(reg, TypeName, _1, _2, _3, _4, _5, _6, _7, _8) \
    FASTRULES_BIND_FIELD(reg, TypeName, _1) \
    FASTRULES_BIND_FIELD(reg, TypeName, _2) \
    FASTRULES_BIND_FIELD(reg, TypeName, _3) \
    FASTRULES_BIND_FIELD(reg, TypeName, _4) \
    FASTRULES_BIND_FIELD(reg, TypeName, _5) \
    FASTRULES_BIND_FIELD(reg, TypeName, _6) \
    FASTRULES_BIND_FIELD(reg, TypeName, _7) \
    FASTRULES_BIND_FIELD(reg, TypeName, _8)

// --- Methods ---
#define FASTRULES_DISPATCH_METHODS_1(reg, TypeName, _1) \
    FASTRULES_BIND_METHOD(reg, TypeName, _1)

#define FASTRULES_DISPATCH_METHODS_2(reg, TypeName, _1, _2) \
    FASTRULES_BIND_METHOD(reg, TypeName, _1) \
    FASTRULES_BIND_METHOD(reg, TypeName, _2)

#define FASTRULES_DISPATCH_METHODS_3(reg, TypeName, _1, _2, _3) \
    FASTRULES_BIND_METHOD(reg, TypeName, _1) \
    FASTRULES_BIND_METHOD(reg, TypeName, _2) \
    FASTRULES_BIND_METHOD(reg, TypeName, _3)

#define FASTRULES_DISPATCH_METHODS_4(reg, TypeName, _1, _2, _3, _4) \
    FASTRULES_BIND_METHOD(reg, TypeName, _1) \
    FASTRULES_BIND_METHOD(reg, TypeName, _2) \
    FASTRULES_BIND_METHOD(reg, TypeName, _3) \
    FASTRULES_BIND_METHOD(reg, TypeName, _4)

// ============================================================================
// MSVC-compatible helper: expand __VA_ARGS__ through an intermediate macro
// ============================================================================

#define FASTRULES_REGISTER_TYPE_A(engine, TypeName, ...) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { \
        FASTRULES_DISPATCH_FIELDS_A(reg, TypeName, __VA_ARGS__) \
    })

#define FASTRULES_DISPATCH_FIELDS_A(reg, TypeName, ...) \
    FASTRULES_DISPATCH_FIELDS_B(reg, TypeName, __VA_ARGS__)

#define FASTRULES_DISPATCH_FIELDS_B(reg, TypeName, ...) \
    FASTRULES_DISPATCH_FIELDS(__VA_ARGS__)(reg, TypeName, __VA_ARGS__)

// Need another level for __VA_ARGS__ to expand before token pasting
#define FASTRULES_DISPATCH_FIELDS_C(...) FASTRULES_DISPATCH_FIELDS(__VA_ARGS__)

// Hmm, MSVC still won't expand __VA_ARGS__ before token pasting.
// Alternative: use explicit per-count macros that are user-facing.

// ============================================================================
// Public API: explicit-count macros (MSVC-safe)
// ============================================================================

// Usage: FASTRULES_REGISTER_TYPE_N(engine, Customer, name, age, balance)  // up to 8 fields
#define FASTRULES_REGISTER_TYPE_1(engine, TypeName, _1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_1(reg, TypeName, _1) })
#define FASTRULES_REGISTER_TYPE_2(engine, TypeName, _1, _2) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_2(reg, TypeName, _1, _2) })
#define FASTRULES_REGISTER_TYPE_3(engine, TypeName, _1, _2, _3) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_3(reg, TypeName, _1, _2, _3) })
#define FASTRULES_REGISTER_TYPE_4(engine, TypeName, _1, _2, _3, _4) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_4(reg, TypeName, _1, _2, _3, _4) })
#define FASTRULES_REGISTER_TYPE_5(engine, TypeName, _1, _2, _3, _4, _5) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_5(reg, TypeName, _1, _2, _3, _4, _5) })
#define FASTRULES_REGISTER_TYPE_6(engine, TypeName, _1, _2, _3, _4, _5, _6) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_6(reg, TypeName, _1, _2, _3, _4, _5, _6) })
#define FASTRULES_REGISTER_TYPE_7(engine, TypeName, _1, _2, _3, _4, _5, _6, _7) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_7(reg, TypeName, _1, _2, _3, _4, _5, _6, _7) })
#define FASTRULES_REGISTER_TYPE_8(engine, TypeName, _1, _2, _3, _4, _5, _6, _7, _8) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_8(reg, TypeName, _1, _2, _3, _4, _5, _6, _7, _8) })

// Usage: FASTRULES_REGISTER_METHODS_N(engine, Customer, isPremium, getTier)  // up to 4 methods
#define FASTRULES_REGISTER_METHODS_1(engine, TypeName, _1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_METHODS_1(reg, TypeName, _1) })
#define FASTRULES_REGISTER_METHODS_2(engine, TypeName, _1, _2) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_METHODS_2(reg, TypeName, _1, _2) })
#define FASTRULES_REGISTER_METHODS_3(engine, TypeName, _1, _2, _3) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_METHODS_3(reg, TypeName, _1, _2, _3) })
#define FASTRULES_REGISTER_METHODS_4(engine, TypeName, _1, _2, _3, _4) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_METHODS_4(reg, TypeName, _1, _2, _3, _4) })

// ============================================================================
// Variadic wrappers (may not work on all MSVC versions; prefer explicit-count)
// ============================================================================

#ifdef _MSC_VER
#pragma warning(push)
// C4003: not enough arguments for function-like macro invocation
// C4002: too many arguments for function-like macro invocation
#pragma warning(disable : 4003 4002)
#endif

// Attempt at variadic dispatch using an intermediate macro layer.
// On compilers with a traditional (non-conformant) preprocessor this may fail.
// In that case use the FASTRULES_REGISTER_TYPE_N explicit-count macros above.

#define FASTRULES_VARG_EXPAND(x) x

#define FASTRULES_VARG_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

#define FASTRULES_VARG_DISPATCH_FIELDS_I(reg, TypeName, ...) \
    FASTRULES_VARG_EXPAND(FASTRULES_VARG_GET_MACRO(__VA_ARGS__, \
        FASTRULES_DISPATCH_FIELDS_8, FASTRULES_DISPATCH_FIELDS_7, FASTRULES_DISPATCH_FIELDS_6, \
        FASTRULES_DISPATCH_FIELDS_5, FASTRULES_DISPATCH_FIELDS_4, FASTRULES_DISPATCH_FIELDS_3, \
        FASTRULES_DISPATCH_FIELDS_2, FASTRULES_DISPATCH_FIELDS_1)(reg, TypeName, __VA_ARGS__))

#define FASTRULES_VARG_DISPATCH_FIELDS(reg, TypeName, ...) \
    FASTRULES_VARG_DISPATCH_FIELDS_I(reg, TypeName, __VA_ARGS__)

#define FASTRULES_VARG_DISPATCH_METHODS_I(reg, TypeName, ...) \
    FASTRULES_VARG_EXPAND(FASTRULES_VARG_GET_MACRO(__VA_ARGS__, \
        FASTRULES_DISPATCH_METHODS_4, FASTRULES_DISPATCH_METHODS_4, FASTRULES_DISPATCH_METHODS_3, \
        FASTRULES_DISPATCH_METHODS_3, FASTRULES_DISPATCH_METHODS_2, FASTRULES_DISPATCH_METHODS_2, \
        FASTRULES_DISPATCH_METHODS_1, FASTRULES_DISPATCH_METHODS_1)(reg, TypeName, __VA_ARGS__))

#define FASTRULES_VARG_DISPATCH_METHODS(reg, TypeName, ...) \
    FASTRULES_VARG_DISPATCH_METHODS_I(reg, TypeName, __VA_ARGS__)

// Variadic public API
#define FASTRULES_REGISTER_TYPE(engine, TypeName, ...) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { \
        FASTRULES_VARG_DISPATCH_FIELDS(reg, TypeName, __VA_ARGS__); \
    })

#define FASTRULES_REGISTER_METHODS(engine, TypeName, ...) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { \
        FASTRULES_VARG_DISPATCH_METHODS(reg, TypeName, __VA_ARGS__); \
    })

// Combined macros -- register fields AND methods in a single registerType call.
// These generate one lambda that binds both fields and methods.
//
// Usage: FASTRULES_REGISTER_TYPE_WITH_METHODS_2_1(engine, Order, id, total, isShipped)
//   (2 fields, 1 method)

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_1_1(engine, TypeName, f1, m1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_1(reg, TypeName, f1) FASTRULES_DISPATCH_METHODS_1(reg, TypeName, m1) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_2_1(engine, TypeName, f1, f2, m1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_2(reg, TypeName, f1, f2) FASTRULES_DISPATCH_METHODS_1(reg, TypeName, m1) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_3_1(engine, TypeName, f1, f2, f3, m1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_3(reg, TypeName, f1, f2, f3) FASTRULES_DISPATCH_METHODS_1(reg, TypeName, m1) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_4_1(engine, TypeName, f1, f2, f3, f4, m1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_4(reg, TypeName, f1, f2, f3, f4) FASTRULES_DISPATCH_METHODS_1(reg, TypeName, m1) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_5_1(engine, TypeName, f1, f2, f3, f4, f5, m1) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_5(reg, TypeName, f1, f2, f3, f4, f5) FASTRULES_DISPATCH_METHODS_1(reg, TypeName, m1) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_2_2(engine, TypeName, f1, f2, m1, m2) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_2(reg, TypeName, f1, f2) FASTRULES_DISPATCH_METHODS_2(reg, TypeName, m1, m2) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_3_2(engine, TypeName, f1, f2, f3, m1, m2) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_3(reg, TypeName, f1, f2, f3) FASTRULES_DISPATCH_METHODS_2(reg, TypeName, m1, m2) })

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_4_2(engine, TypeName, f1, f2, f3, f4, m1, m2) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_4(reg, TypeName, f1, f2, f3, f4) FASTRULES_DISPATCH_METHODS_2(reg, TypeName, m1, m2) })

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// End of type_registration_macro.hpp

#define FASTRULES_REGISTER_TYPE_WITH_METHODS_5_2(engine, TypeName, f1, f2, f3, f4, f5, m1, m2) \
    engine.registerType<TypeName>(#TypeName, [](auto& reg) { FASTRULES_DISPATCH_FIELDS_5(reg, TypeName, f1, f2, f3, f4, f5) FASTRULES_DISPATCH_METHODS_2(reg, TypeName, m1, m2) })
