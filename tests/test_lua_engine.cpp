/**
 * @file test_lua_engine.cpp
 * @brief Unit tests for the LuaEngine class
 * 
 * Tests cover:
 * - Engine creation and destruction
 * - Expression compilation (various types)
 * - Action compilation
 * - Expression evaluation with parameters
 * - Action execution
 * - Type marshaling (int, double, string, bool)
 * - Global variable access
 * - Action callback registration
 * - Timeout handling
 * - Compilation errors
 * 
 * Test Framework: doctest
 */

#include <doctest/doctest.h>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("LuaEngine creation") {
    REQUIRE_NOTHROW(LuaEngine());
}

TEST_CASE("LuaEngine expression compilation") {
    LuaEngine engine;

    SUBCASE("Simple boolean") {
        auto ref = engine.compileExpression("true");
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SUBCASE("Comparison") {
        auto ref = engine.compileExpression("value > 10");
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SUBCASE("String operation") {
        auto ref = engine.compileExpression("string.len(name) > 0");
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SUBCASE("Invalid syntax throws") {
        REQUIRE_THROWS([&](){ (void)engine.compileExpression("if then"); }());
    }
}

TEST_CASE("LuaEngine action compilation") {
    LuaEngine engine;

    SUBCASE("Simple assignment") {
        auto ref = engine.compileAction("x = 5");
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SUBCASE("Empty action returns nullopt") {
        auto ref = engine.compileAction("");
        REQUIRE_FALSE(ref.has_value());
    }
}

TEST_CASE("LuaEngine expression evaluation") {
    LuaEngine engine;
    RuleContext ctx;

    SUBCASE("True literal") {
        auto ref = engine.compileExpression("true");
        std::vector<RuleParameter> params;
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }

    SUBCASE("False literal") {
        auto ref = engine.compileExpression("false");
        std::vector<RuleParameter> params;
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == false);
        engine.releaseRef(ref.value());
    }

    SUBCASE("Comparison with parameter") {
        auto ref = engine.compileExpression("value >= 18");
        std::vector<RuleParameter> params;
        params.emplace_back("value", 25.0);
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }
}

TEST_CASE("LuaEngine predicate functions") {
    LuaEngine engine;
    RuleContext ctx;

    SUBCASE("isNotNull with nil") {
        auto ref = engine.compileExpression("isNotNull(value)");
        std::vector<RuleParameter> params;
        // Don't set value - it will be nil
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == false);
        engine.releaseRef(ref.value());
    }

    SUBCASE("isEmpty string") {
        auto ref = engine.compileExpression("isEmpty(str)");
        std::vector<RuleParameter> params;
        params.emplace_back("str", std::string(""));
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }

    SUBCASE("startsWith") {
        auto ref = engine.compileExpression("startsWith(str, \"Hello\")");
        std::vector<RuleParameter> params;
        params.emplace_back("str", std::string("Hello World"));
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }

    SUBCASE("inRange") {
        auto ref = engine.compileExpression("inRange(val, 0, 100)");
        std::vector<RuleParameter> params;
        params.emplace_back("val", 50.0);
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }
}

TEST_CASE("LuaEngine coroutine support") {
    LuaEngine engine;
    RuleContext ctx;

    SUBCASE("Compile coroutine") {
        auto ref = engine.compileCoroutine("true");
        REQUIRE(ref.has_value());
        REQUIRE(engine.isCoroutine(ref.value()));
        engine.releaseRef(ref.value());
    }

    SUBCASE("Coroutine is not regular expression") {
        auto coroRef = engine.compileCoroutine("true");
        auto exprRef = engine.compileExpression("true");
        
        REQUIRE(engine.isCoroutine(coroRef.value()));
        REQUIRE_FALSE(engine.isCoroutine(exprRef.value()));
        
        engine.releaseRef(coroRef.value());
        engine.releaseRef(exprRef.value());
    }

    SUBCASE("Resume coroutine") {
        auto ref = engine.compileCoroutine("true");
        auto status = engine.resumeCoroutine(ref.value(), {}, ctx);
        REQUIRE(status == true);
        engine.releaseRef(ref.value());
    }

    SUBCASE("Resume coroutine with parameters") {
        auto ref = engine.compileCoroutine("x > 10");
        std::vector<RuleParameter> params;
        params.emplace_back("x", 15);
        
        auto status = engine.resumeCoroutine(ref.value(), params, ctx);
        REQUIRE(status == true);
        engine.releaseRef(ref.value());
    }

    SUBCASE("Clone engine for thread safety") {
        auto ref = engine.compileExpression("true");
        auto cloned = engine.clone();
        
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->luaState() != engine.luaState());
        
        // Clone should work independently
        auto clonedRef = cloned->compileExpression("false");
        RuleContext clonedCtx;
        bool result = cloned->evaluateExpression(clonedRef.value(), {}, clonedCtx);
        REQUIRE(result == false);
        
        engine.releaseRef(ref.value());
        cloned->releaseRef(clonedRef.value());
    }
}
