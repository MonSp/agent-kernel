// Additional tests for SimulationRunner — edge cases and error handling.
#include "ecs/systems/SimulationRunner.h"
#include "llm/HttpClient.h"
#include "llm/LLMClient.h"
#include "ecs/Registry.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace Systems;
using namespace ECS;

static void mockLLMAction(const std::string& content) {
    std::string escaped;
    for (char c : content) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            default:   escaped += c;      break;
        }
    }
    std::string body = R"({"choices":[{"message":{"role":"assistant","content":")" + escaped + R"("}}],"usage":{"prompt_tokens":10,"completion_tokens":5}})";
    LLM::HttpClient::setMockResponse(200, body);
}

static EntityId createUnitEntity(const std::string& name, float energy = 70.0f) {
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), name, AgentRole::Specialist);
    auto& social = reg.addComponent<SocialComponent>(id);
    social.energy = energy;
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Intermediate);
    reg.addComponent<CareerComponent>(id);
    reg.addComponent<PersonalityComponent>(id, 50, 50, 50, 50, 50, 50);
    reg.addComponent<MemoryRingComponent>(id);
    return id;
}

// ─── Edge cases ──────────────────────────────────────────────────────────────

static void testRunEmptyEntityList() {
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    auto results = runner.run(Registry::getInstance(), {}, 5, {"task"});
    assert(results.empty());
    printf("  PASS: testRunEmptyEntityList\n");
}

static void testRunZeroTicks() {
    mockLLMAction(R"({"action":"execute","reasoning":"ok","confidence":0.7})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createUnitEntity("Zero");
    auto results = runner.run(Registry::getInstance(), {id}, 0, {"task"});
    assert(results.empty());
    printf("  PASS: testRunZeroTicks\n");
}

static void testRunEmptyTasks() {
    mockLLMAction(R"({"action":"execute","reasoning":"ok","confidence":0.7})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createUnitEntity("NoTask");
    auto results = runner.run(Registry::getInstance(), {id}, 2, {});
    assert(results.size() == 2);
    printf("  PASS: testRunEmptyTasks\n");
}

static void testTickNumberingSequential() {
    mockLLMAction(R"({"action":"execute","reasoning":"ok","confidence":0.7})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createUnitEntity("Seq");
    auto results = runner.run(Registry::getInstance(), {id}, 5, {"task"});
    assert(results.size() == 5);
    for (size_t i = 0; i < results.size(); ++i) {
        assert(results[i].tickNumber == static_cast<int>(i));
    }
    printf("  PASS: testTickNumberingSequential\n");
}

static void testSummaryEmptyResults() {
    auto summary = SimulationRunner::summarize({});
    assert(summary.totalTicks == 0);
    assert(summary.actionCounts.empty());
    printf("  PASS: testSummaryEmptyResults\n");
}

static void testSummaryActionCounts() {
    mockLLMAction(R"({"action":"execute","reasoning":"ok","confidence":0.8})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createUnitEntity("Counts");
    auto results = runner.run(Registry::getInstance(), {id}, 4, {"task"});
    auto summary = SimulationRunner::summarize(results);
    assert(summary.totalTicks == 4);
    assert(!summary.actionCounts.empty());
    // Total action counts should equal total ticks
    int totalActions = 0;
    for (auto& [action, count] : summary.actionCounts) {
        totalActions += count;
    }
    assert(totalActions == 4);
    printf("  PASS: testSummaryActionCounts\n");
}

static void testSummaryConfidenceRange() {
    mockLLMAction(R"({"action":"execute","reasoning":"ok","confidence":0.6})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createUnitEntity("Conf");
    auto results = runner.run(Registry::getInstance(), {id}, 3, {"task"});
    auto summary = SimulationRunner::summarize(results);
    assert(summary.averageConfidence >= 0.0f && summary.averageConfidence <= 1.0f);
    printf("  PASS: testSummaryConfidenceRange\n");
}

static void testSummaryToJsonContent() {
    SimulationSummary s;
    s.totalTicks = 15;
    s.averageConfidence = 0.82f;
    s.actionCounts["executeTask"] = 10;
    s.actionCounts["rest"] = 5;
    std::string json = s.toJson();
    assert(json.find("15") != std::string::npos);
    assert(json.find("executeTask") != std::string::npos);
    assert(json.find("rest") != std::string::npos);
    printf("  PASS: testSummaryToJsonContent\n");
}

static void testLowEnergyAgent() {
    mockLLMAction(R"({"action":"reflect","reasoning":"low energy","confidence":0.6})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    EntityId id = createUnitEntity("LowEnergy", 5.0f);
    auto results = runner.run(Registry::getInstance(), {id}, 2, {"task"});
    assert(results.size() == 2);
    printf("  PASS: testLowEnergyAgent\n");
}

static void testManyEntities() {
    mockLLMAction(R"({"action":"execute","reasoning":"ok","confidence":0.7})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    SimulationRunner runner(&engine);
    Registry::getInstance().clear();
    std::vector<EntityId> ids;
    for (int i = 0; i < 10; ++i) {
        ids.push_back(createUnitEntity("Agent" + std::to_string(i)));
    }
    auto results = runner.run(Registry::getInstance(), ids, 2, {"task"});
    assert(results.size() == 20);  // 10 entities × 2 ticks
    printf("  PASS: testManyEntities\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runSimulationRunnerUnitTests() {
    printf("Running SimulationRunner unit tests...\n");

    testRunEmptyEntityList();
    testRunZeroTicks();
    testRunEmptyTasks();
    testTickNumberingSequential();
    testSummaryEmptyResults();
    testSummaryActionCounts();
    testSummaryConfidenceRange();
    testSummaryToJsonContent();
    testLowEnergyAgent();
    testManyEntities();

    printf("All 10 SimulationRunner unit tests PASSED.\n");
}
