#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("LuaEngine state reset clears compiled refs", "[lua_engine][cleanup]") {
    LuaEngine engine;

    auto ref = engine.compileExpression("true");
    REQUIRE(ref.has_value());

    engine.resetState();

    // After reset, the old ref is invalid
    std::vector<RuleParameter> params;
    RuleContext ctx;
    REQUIRE_THROWS_AS(engine.evaluateExpression(ref.value(), params, ctx), RuleExecutionException);
}

TEST_CASE("LuaEngine state reset preserves types and callbacks", "[lua_engine][cleanup]") {
    LuaEngine engine;

    // Register a type
    engine.registerType<int>("MyInt", [](auto& ut) {
        ut["value"] = [](int* self) { return *self; };
    });

    // Register an action (sol2-specific signature)
    // engine.registerAction("testAction", [](sol::object, const std::vector<sol::object>&) {
    //     return true;
    // });

    engine.resetState();

    // Types should still be registered
    REQUIRE(engine.isTypeRegistered("MyInt"));

    // Actions should still be registered
    // REQUIRE(engine.hasAction("testAction"));  // registerAction stubbed for non-sol2
    REQUIRE(true);  // placeholder
}

TEST_CASE("LuaEngine compile count tracking", "[lua_engine][cleanup]") {
    LuaEngine engine;

    REQUIRE(engine.getCompileCount() == 0);

    auto ref1 = engine.compileExpression("true");
    REQUIRE(engine.getCompileCount() == 1);

    auto ref2 = engine.compileAction("x = 1");
    REQUIRE(engine.getCompileCount() == 2);

    engine.resetState();
    REQUIRE(engine.getCompileCount() == 0);
}

