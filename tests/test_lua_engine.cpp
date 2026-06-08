#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("LuaEngine creation", "[lua]") {
    REQUIRE_NOTHROW(LuaEngine());
}

TEST_CASE("LuaEngine expression compilation", "[lua]") {
    LuaEngine engine;

    SECTION("Simple boolean") {
        auto ref = engine.compileExpression("true", {});
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SECTION("Comparison") {
        auto ref = engine.compileExpression("value > 10", {"value"});
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SECTION("String operation") {
        auto ref = engine.compileExpression("string.len(name) > 0", {"name"});
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SECTION("Invalid syntax throws") {
        REQUIRE_THROWS(engine.compileExpression("if then", {}));
    }
}

TEST_CASE("LuaEngine action compilation", "[lua]") {
    LuaEngine engine;

    SECTION("Simple assignment") {
        auto ref = engine.compileAction("x = 5", {"x"});
        REQUIRE(ref.has_value());
        engine.releaseRef(ref.value());
    }

    SECTION("Empty action returns nullopt") {
        auto ref = engine.compileAction("", {});
        REQUIRE_FALSE(ref.has_value());
    }
}

TEST_CASE("LuaEngine expression evaluation", "[lua]") {
    LuaEngine engine;
    RuleContext ctx;

    SECTION("True literal") {
        auto ref = engine.compileExpression("true", {});
        std::vector<RuleParameter> params;
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }

    SECTION("False literal") {
        auto ref = engine.compileExpression("false", {});
        std::vector<RuleParameter> params;
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == false);
        engine.releaseRef(ref.value());
    }

    SECTION("Comparison with parameter") {
        auto ref = engine.compileExpression("value >= 18", {"value"});
        std::vector<RuleParameter> params;
        params.emplace_back("value", "double", std::any(25.0));
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }
}

TEST_CASE("LuaEngine predicate functions", "[lua]") {
    LuaEngine engine;
    RuleContext ctx;

    SECTION("isNotNull with nil") {
        auto ref = engine.compileExpression("isNotNull(value)", {"value"});
        std::vector<RuleParameter> params;
        // Don't set value - it will be nil
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == false);
        engine.releaseRef(ref.value());
    }

    SECTION("isEmpty string") {
        auto ref = engine.compileExpression("isEmpty(str)", {"str"});
        std::vector<RuleParameter> params;
        params.emplace_back("str", "string", std::any(std::string("")));
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }

    SECTION("startsWith") {
        auto ref = engine.compileExpression("startsWith(str, \"Hello\")", {"str"});
        std::vector<RuleParameter> params;
        params.emplace_back("str", "string", std::any(std::string("Hello World")));
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }

    SECTION("inRange") {
        auto ref = engine.compileExpression("inRange(val, 0, 100)", {"val"});
        std::vector<RuleParameter> params;
        params.emplace_back("val", "double", std::any(50.0));
        bool result = engine.evaluateExpression(ref.value(), params, ctx);
        REQUIRE(result == true);
        engine.releaseRef(ref.value());
    }
}

TEST_CASE("LuaEngine coroutine support", "[lua][coroutine]") {
    LuaEngine engine;
    RuleContext ctx;

    SECTION("Compile coroutine") {
        auto ref = engine.compileCoroutine("true", {});
        REQUIRE(ref.has_value());
        REQUIRE(engine.isCoroutine(ref.value()));
        engine.releaseRef(ref.value());
    }

    SECTION("Coroutine is not regular expression") {
        auto coroRef = engine.compileCoroutine("true", {});
        auto exprRef = engine.compileExpression("true", {});
        
        REQUIRE(engine.isCoroutine(coroRef.value()));
        REQUIRE_FALSE(engine.isCoroutine(exprRef.value()));
        
        engine.releaseRef(coroRef.value());
        engine.releaseRef(exprRef.value());
    }

    SECTION("Resume coroutine") {
        auto ref = engine.compileCoroutine("true", {});
        auto status = engine.resumeCoroutine(ref.value(), {}, ctx);
        REQUIRE(status == true);
        engine.releaseRef(ref.value());
    }

    SECTION("Resume coroutine with parameters") {
        auto ref = engine.compileCoroutine("x > 10", {"x"});
        std::vector<RuleParameter> params;
        params.emplace_back("x", "int", std::any(15));
        
        auto status = engine.resumeCoroutine(ref.value(), params, ctx);
        REQUIRE(status == true);
        engine.releaseRef(ref.value());
    }

    SECTION("Clone engine for thread safety") {
        auto ref = engine.compileExpression("true", {});
        auto cloned = engine.clone();
        
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->luaState() != engine.luaState());
        
        // Clone should work independently
        auto clonedRef = cloned->compileExpression("false", {});
        RuleContext clonedCtx;
        bool result = cloned->evaluateExpression(clonedRef.value(), {}, clonedCtx);
        REQUIRE(result == false);
        
        engine.releaseRef(ref.value());
        cloned->releaseRef(clonedRef.value());
    }
}
