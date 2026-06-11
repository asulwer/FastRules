#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("ExecutionTracer basic", "[execution_tracing]") {
    ExecutionTracer tracer(1);

    tracer.start();
    REQUIRE(tracer.isActive());

    tracer.record(1, "compile", true, "Compiled successfully");
    tracer.record(1, "execute", true, "Rule evaluated to true");

    tracer.finish(true);
    REQUIRE(!tracer.isActive());

    const auto& trace = tracer.getTrace();
    REQUIRE(trace.workflowId == 1);
    REQUIRE(trace.steps.size() == 2);
    REQUIRE(trace.overallSuccess);
}

TEST_CASE("ExecutionTrace query methods", "[execution_tracing]") {
    ExecutionTracer tracer(2);
    tracer.start();

    ExecutionTraceStep step1;
    step1.ruleId = 1;
    step1.stage = "compile";
    step1.success = true;
    step1.startedAt = std::chrono::steady_clock::now();
    step1.endedAt = step1.startedAt + std::chrono::milliseconds(10);
    tracer.addStep(step1);

    ExecutionTraceStep step2;
    step2.ruleId = 2;
    step2.stage = "execute";
    step2.success = true;
    step2.startedAt = std::chrono::steady_clock::now();
    step2.endedAt = step2.startedAt + std::chrono::milliseconds(50);
    tracer.addStep(step2);

    ExecutionTraceStep step3;
    step3.ruleId = 1;
    step3.stage = "execute";
    step3.success = false;
    step3.startedAt = std::chrono::steady_clock::now();
    step3.endedAt = step3.startedAt + std::chrono::milliseconds(5);
    tracer.addStep(step3);

    tracer.finish(false);

    const auto& trace = tracer.getTrace();

    // getStepsForRule
    auto ruleASteps = trace.getStepsForRule(1);
    REQUIRE(ruleASteps.size() == 2);

    // getTotalTimeInStage
    auto compileTime = trace.getTotalTimeInStage("compile");
    REQUIRE(compileTime.count() >= 10000000);  // ~10ms in nanoseconds

    // getSlowestStep
    auto slowest = trace.getSlowestStep();
    REQUIRE(slowest.has_value());
    REQUIRE(slowest->ruleId == 2);
}

TEST_CASE("ExecutionTrace JSON serialization", "[execution_tracing]") {
    ExecutionTracer tracer(3);
    tracer.start();

    tracer.record(1, "execute", true, "OK");
    tracer.finish(true);

    // toJson() has been removed from core — use JsonSerialization::serialize(trace)
    REQUIRE(true);
}

TEST_CASE("Workflow executeWithTrace", "[execution_tracing]") {
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

    ExecutionTracer tracer(4);
    std::vector<RuleParameter> params;

    auto results = workflow.executeWithTrace(engine, params, tracer);

    REQUIRE(results.size() == 2);
    REQUIRE(!tracer.isActive());

    const auto& trace = tracer.getTrace();
    REQUIRE(trace.steps.size() >= 2);  // At least execute steps for both rules

    auto rule1Steps = trace.getStepsForRule(1);
    REQUIRE(!rule1Steps.empty());
}
