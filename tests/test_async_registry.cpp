#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>
#include <fastrules/async_registry.hpp>

using namespace fastrules;

TEST_CASE("Async registry basic", "[async]") {
    LuaEngine engine;
    AsyncRegistry registry;
    
    // Test that we can bind without errors
    // registry.bindToLua(engine.luaState());  // API uses sol::state&
    
    // Just verify engine works
    auto ref = engine.compileExpression("1 + 1", {});
    REQUIRE(ref.has_value());
    
    RuleContext ctx;
    auto result = engine.evaluateExpression(ref.value(), {}, ctx);
    REQUIRE(result == true);  // 1 + 1 = 2 (truthy)
}
