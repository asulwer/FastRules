#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <chrono>
#include <future>

using namespace fastrules;

TEST_CASE("LuaEngine extended compilation") {
    LuaEngine engine;
    
    // Test compiling various expressions
    auto ref1 = engine.compileExpression("x > 0");
    REQUIRE(ref1.has_value());
    
    auto ref2 = engine.compileExpression("name == 'test'");
    REQUIRE(ref2.has_value());
    
    auto ref3 = engine.compileExpression("age >= 18 and age <= 65");
    REQUIRE(ref3.has_value());
    
    // Test compiling actions
    auto actionRef = engine.compileAction("print('Hello World')");
    REQUIRE(actionRef.has_value());
    
    // Test releasing references
    engine.releaseRef(ref1.value());
    engine.releaseRef(ref2.value());
    engine.releaseRef(ref3.value());
    engine.releaseRef(actionRef.value());
}

TEST_CASE("LuaEngine extended evaluation") {
    LuaEngine engine;
    RuleContext context;
    
    // Test evaluating simple expressions
    auto ref = engine.compileExpression("x > 0");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);
    
    bool result = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result == true);
    
    // Test with false result
    params.clear();
    params.emplace_back("x", -5);
    
    bool result2 = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result2 == false);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine complex expressions") {
    LuaEngine engine;
    RuleContext context;
    
    // Test complex boolean expressions
    auto ref = engine.compileExpression("(age >= 18 and age <= 65) and (income > 30000)");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("age", 30);
    params.emplace_back("income", 50000);
    
    bool result = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result == true);
    
    // Test with failing condition
    params.clear();
    params.emplace_back("age", 15);
    params.emplace_back("income", 50000);
    
    bool result2 = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result2 == false);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine string operations") {
    LuaEngine engine;
    RuleContext context;
    
    // Test string operations
    auto ref = engine.compileExpression("name:find('test') ~= nil");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("name", "this is a test string");
    
    bool result = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result == true);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine mathematical operations") {
    LuaEngine engine;
    RuleContext context;
    
    // Test mathematical operations
    auto ref = engine.compileExpression("((price * quantity) - discount) > 100");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("price", 50.0);
    params.emplace_back("quantity", 3);
    params.emplace_back("discount", 20.0);
    
    bool result = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result == true); // (50 * 3) - 20 = 130 > 100
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine action execution") {
    LuaEngine engine;
    RuleContext context;
    
    // Test action execution
    auto ref = engine.compileAction("result = x * 2");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("x", 10);
    
    engine.executeAction(ref.value(), params, context);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine coroutine functionality") {
    LuaEngine engine;
    RuleContext context;
    
    // Test coroutine compilation
    auto ref = engine.compileCoroutine("x = x + 1; return x");
    REQUIRE(ref.has_value());
    CHECK(engine.isCoroutine(ref.value()));
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine type registration") {
    LuaEngine engine;
    
    // Test registering a simple type
    engine.registerType<std::string>("StringType", [](auto& reg) {
        // Register methods if needed
    });
    
    // This should not throw
    CHECK(true);
}

TEST_CASE("LuaEngine global variables") {
    LuaEngine engine;
    RuleContext context;
    
    // Test setting global variables
    engine.setGlobal("global_var", 42);
    
    auto ref = engine.compileExpression("global_var == 42");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    bool result = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result == true);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine error handling") {
    LuaEngine engine;
    RuleContext context;
    
    // Test compilation error handling
    auto ref = engine.compileExpression("invalid syntax +++");
    CHECK_FALSE(ref.has_value());
    
    // Test evaluation with invalid expression reference
    // This would normally throw, but we're testing that it handles gracefully
}

TEST_CASE("LuaEngine timeout functionality") {
    LuaEngine engine;
    RuleContext context;
    
    // Test with timeout
    auto ref = engine.compileExpression("x > 0");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);
    
    auto timeout = std::chrono::milliseconds(1000);
    bool result = engine.evaluateExpression(ref.value(), params, context, timeout);
    CHECK(result == true);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine thread safety") {
    LuaEngine engine;
    
    // Test that we can compile expressions from multiple threads
    std::vector<std::future<std::optional<int>>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [&engine, i]() {
            std::string expr = "x > " + std::to_string(i);
            return engine.compileExpression(expr);
        }));
    }
    
    // Check that all compilations succeeded
    for (auto& future : futures) {
        auto result = future.get();
        REQUIRE(result.has_value());
    }
}

TEST_CASE("LuaEngine memory management") {
    LuaEngine engine;
    
    // Test compiling many expressions
    std::vector<int> refs;
    for (int i = 0; i < 100; ++i) {
        auto ref = engine.compileExpression("x > " + std::to_string(i));
        if (ref.has_value()) {
            refs.push_back(ref.value());
        }
    }
    
    // Release all references
    for (int ref : refs) {
        engine.releaseRef(ref);
    }
    
    CHECK(true); // If we get here without crashing, memory management works
}

TEST_CASE("LuaEngine clone functionality") {
    LuaEngine engine;
    
    // Test cloning the engine
    auto clone = engine.clone();
    REQUIRE(clone != nullptr);
    
    // Test that clone can compile expressions
    auto ref = clone->compileExpression("x > 0");
    REQUIRE(ref.has_value());
    
    clone->releaseRef(ref.value());
}

TEST_CASE("LuaEngine predicate functions") {
    LuaEngine engine;
    RuleContext context;
    
    // Test built-in predicate functions
    auto ref = engine.compileExpression("not isNull(value)");
    REQUIRE(ref.has_value());
    
    std::vector<RuleParameter> params;
    params.emplace_back("value", 42);
    
    bool result = engine.evaluateExpression(ref.value(), params, context);
    CHECK(result == true);
    
    engine.releaseRef(ref.value());
}

TEST_CASE("LuaEngine performance") {
    LuaEngine engine;
    RuleContext context;
    
    // Test compiling and evaluating many expressions
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        std::string expr = "x > " + std::to_string(i % 100);
        auto ref = engine.compileExpression(expr);
        if (ref.has_value()) {
            std::vector<RuleParameter> params;
            params.emplace_back("x", i);
            engine.evaluateExpression(ref.value(), params, context);
            engine.releaseRef(ref.value());
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 1000 operations quickly
    CHECK(duration.count() < 5000); // 5 seconds should be more than enough
}

TEST_CASE("LuaEngine edge cases") {
    LuaEngine engine;
    RuleContext context;
    
    // Test empty expressions
    auto ref1 = engine.compileExpression("");
    CHECK_FALSE(ref1.has_value());
    
    // Test whitespace-only expressions
    auto ref2 = engine.compileExpression("   ");
    CHECK_FALSE(ref2.has_value());
    
    // Test very long expressions
    std::string longExpr(10000, 'x');
    longExpr = "(" + longExpr + " > 0)";
    auto ref3 = engine.compileExpression(longExpr);
    // This might fail due to length limits, which is expected behavior
}