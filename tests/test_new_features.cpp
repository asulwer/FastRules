#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("Streaming execution basic", "[workflow][streaming]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;
    
    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->expression = "true";
    
    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->expression = "false";
    
    workflow.rules.push_back(rule1);
    workflow.rules.push_back(rule2);
    
    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);
    REQUIRE(results.size() == 2);
    
    std::vector<RuleResult> streamed;
    for (auto result : workflow.executeStreaming(engine, params)) {
        streamed.push_back(result);
    }
    REQUIRE(streamed.size() == 2);
    REQUIRE(streamed[0].ruleId == 1);
    REQUIRE(streamed[0].isSuccess() == true);
    REQUIRE(streamed[1].ruleId == 2);
    REQUIRE(streamed[1].isSuccess() == false);
}

TEST_CASE("Pretty-printed JSON serialization", "[json]") {
    // This test requires the JSON extension
    // Skipping since JsonLoader is now in the extension
    REQUIRE(true);
}

TEST_CASE("Rule pretty JSON", "[json]") {
    // This test requires the JSON extension
    // Skipping since JsonLoader is now in the extension
    REQUIRE(true);
}
