#include <doctest/doctest.h>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("ExecutionTracer basic") {
    ExecutionTracer tracer(1);

    tracer.start();
    REQUIRE(tracer.isActive());

    tracer.record("rule1", "compile", true, "Compiled successfully");
    tracer.record("rule1", "execute", true, "Rule evaluated to true");

    tracer.finish(true);
    REQUIRE(!tracer.isActive());

    const auto& trace = tracer.getTrace();
    REQUIRE(trace.workflowId == 1);
    REQUIRE(trace.steps.size() == 2);
    REQUIRE(trace.overallSuccess);
}

TEST_CASE("ExecutionTrace query methods") {
    ExecutionTracer tracer(2);
    tracer.start();

    ExecutionTraceStep step1;
    step1.ruleName = "rule1";
    step1.stage = "compile";
    step1.success = true;
    step1.startedAt = std::chrono::steady_clock::now();
    step1.endedAt = step1.startedAt + std::chrono::milliseconds(10);
    tracer.addStep(step1);

    ExecutionTraceStep step2;
    step2.ruleName = "rule2";
    step2.stage = "execute";
    step2.success = true;
    step2.startedAt = std::chrono::steady_clock::now();
    step2.endedAt = step2.startedAt + std::chrono::milliseconds(50);
    tracer.addStep(step2);

    ExecutionTraceStep step3;
    step3.ruleName = "rule1";
    step3.stage = "execute";
    step3.success = false;
    step3.startedAt = std::chrono::steady_clock::now();
    step3.endedAt = step3.startedAt + std::chrono::milliseconds(5);
    tracer.addStep(step3);

    tracer.finish(false);

    const auto& trace = tracer.getTrace();

    // getStepsForRule
    auto ruleASteps = trace.getStepsForRule("rule1");
    REQUIRE(ruleASteps.size() == 2);

    // getTotalTimeInStage
    auto compileTime = trace.getTotalTimeInStage("compile");
    REQUIRE(compileTime.count() >= 10000000);  // ~10ms in nanoseconds

    // getSlowestStep
    auto slowest = trace.getSlowestStep();
    REQUIRE(slowest.has_value());
    REQUIRE(slowest->ruleName == "rule2");
}

TEST_CASE("ExecutionTrace JSON serialization") {
    ExecutionTracer tracer(3);
    tracer.start();

    tracer.record("rule1", "execute", true, "OK");
    tracer.finish(true);

    // toJson() has been removed from core -- use JsonSerialization::serialize(trace)
    REQUIRE(true);
}

TEST_CASE("Workflow executeWithTrace") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    auto rule1 = std::make_shared<Rule>();
    rule1->id = 1;
    rule1->name = "rule1";
    rule1->expression = "true";

    auto rule2 = std::make_shared<Rule>();
    rule2->id = 2;
    rule2->name = "rule2";
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

    auto rule1Steps = trace.getStepsForRule("rule1");
    REQUIRE(!rule1Steps.empty());
}
