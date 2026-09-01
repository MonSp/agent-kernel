// Tests for SimulationRunner — multi-agent batch simulation.
#include "ecs/systems/SimulationRunner.h"
#include "llm/HttpClient.h"
#include "ecs/Registry.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include <cassert>
#include <cstdio>

using namespace Systems;
using namespace ECS;

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static void mockLLM(const std::string& content) {
    std::string escaped = escapeJson(content);
    std::string body = R"({"choices":[{"message":{"role":"assistant","content":")" + escaped + R"("}}],"usage":{"prompt_tokens":10,"completion_tokens":5}})";
    LLM::HttpClient::setMockResponse(200, body);
}

static EntityId createAgent(const std::string& name, float energy) {
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), name, AgentRole::Specialist);
    auto& social = reg.addComponent<SocialComponent>(id);
    social.energy = energy;
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    reg.addComponent<CareerComponent>(id);
    reg.addComponent<PersonalityComponent>(id, 50, 50, 50, 50, 50, 50);
    reg.addComponent<MemoryRingComponent>(id);
    return id;
}

static void testRunSingleEntity() {
    mockLLM(R"({"action":"execute","reasoning":"ok","confidence":0.7})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createAgent("Agent1", 80.0f);
    auto& reg = Registry::getInstance();

    auto results = runner.run(reg, {id}, 3, {"task A"});
    assert(results.size() == 3);
    assert(results[0].tickNumber == 0);
    assert(results[2].tickNumber == 2);
    printf("  PASS: testRunSingleEntity\n");
}

static void testRunMultipleEntities() {
    mockLLM(R"({"action":"execute","reasoning":"ok","confidence":0.6})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id1 = createAgent("Agent1", 80.0f);
    EntityId id2 = createAgent("Agent2", 70.0f);
    auto& reg = Registry::getInstance();

    auto results = runner.run(reg, {id1, id2}, 2, {"task A", "task B"});
    assert(results.size() == 4);
    printf("  PASS: testRunMultipleEntities\n");
}

static void testSummarize() {
    mockLLM(R"({"action":"execute","reasoning":"ok","confidence":0.8})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createAgent("Agent1", 80.0f);
    auto& reg = Registry::getInstance();

    auto results = runner.run(reg, {id}, 5, {"task"});
    auto summary = SimulationRunner::summarize(results);
    assert(summary.totalTicks == 5);
    assert(summary.averageConfidence > 0.0f);
    assert(!summary.actionCounts.empty());
    printf("  PASS: testSummarize\n");
}

static void testSummaryToJson() {
    SimulationSummary summary;
    summary.totalTicks = 10;
    summary.averageConfidence = 0.75f;
    summary.actionCounts["executeTask"] = 8;
    summary.actionCounts["rest"] = 2;
    std::string json = summary.toJson();
    assert(json.find("10") != std::string::npos);
    assert(json.find("executeTask") != std::string::npos);
    printf("  PASS: testSummaryToJson\n");
}

static void testRunDefaultTask() {
    mockLLM(R"({"action":"execute","reasoning":"ok","confidence":0.5})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createAgent("Agent1", 80.0f);
    auto& reg = Registry::getInstance();

    auto results = runner.run(reg, {id}, 2, {});
    assert(results.size() == 2);
    printf("  PASS: testRunDefaultTask\n");
}

void runSimulationRunnerTests() {
    printf("=== test_simulation_runner ===\n");
    testRunSingleEntity();
    testRunMultipleEntities();
    testSummarize();
    testSummaryToJson();
    testRunDefaultTask();
    printf("All 5 simulation runner tests PASSED.\n");
}
