// Tests for TickEngine — full agent tick cycle with mock LLM.
#include "ecs/systems/TickEngine.h"
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
#include <cmath>

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

static EntityId createFullEntity() {
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), "TestAgent", AgentRole::Specialist);
    auto& social = reg.addComponent<SocialComponent>(id);
    social.energy = 70.0f;
    social.mood = 60.0f;
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    reg.addComponent<CareerComponent>(id);
    reg.addComponent<PersonalityComponent>(id, 50, 50, 50, 50, 50, 50);
    reg.addComponent<MemoryRingComponent>(id);
    return id;
}

static void testTickExecuteTask() {
    mockLLM(R"({"action":"execute","reasoning":"I can handle this","confidence":0.9,"details":"Implement feature X"})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();

    TickResult result = engine.tick(reg, id, "Implement feature X");

    assert(result.action == ActionType::ExecuteTask);
    assert(result.tickNumber == 0);
    assert(result.timestamp > 0);
    assert(!result.effects.empty());
    auto* career = reg.getComponent<CareerComponent>(id);
    assert(career->totalXp > 0);
    printf("  PASS: testTickExecuteTask\n");
}

static void testTickDelegate() {
    mockLLM(R"({"action":"delegate","reasoning":"Not my domain","confidence":0.7,"delegateTo":"frontend-team","details":"CSS work"})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();

    TickResult result = engine.tick(reg, id, "Fix CSS layout");

    assert(result.action == ActionType::Delegate);
    assert(result.effects.size() == 1);
    assert(result.effects[0].target == TargetComponent::Memory);
    printf("  PASS: testTickDelegate\n");
}

static void testTickLowEnergyRest() {
    mockLLM(R"({"action":"execute","reasoning":"Working","confidence":0.5})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    reg.getComponent<SocialComponent>(id)->energy = 15.0f;

    TickResult result = engine.tick(reg, id, "Some task");

    assert(result.action == ActionType::Rest);
    auto* social = reg.getComponent<SocialComponent>(id);
    assert(social->energy > 15.0f);
    printf("  PASS: testTickLowEnergyRest\n");
}

static void testTickCounter() {
    mockLLM(R"({"action":"execute","reasoning":"ok","confidence":0.5})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();

    TickResult r1 = engine.tick(reg, id, "task1");
    TickResult r2 = engine.tick(reg, id, "task2");
    assert(r1.tickNumber == 0);
    assert(r2.tickNumber == 1);
    printf("  PASS: testTickCounter\n");
}

static void testTickResultToJson() {
    mockLLM(R"({"action":"execute","reasoning":"test","confidence":0.8})");
    LLM::LLMClient client(LLM::LLMConfig{});
    TickEngine engine(&client);
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();

    TickResult result = engine.tick(reg, id, "task");
    std::string json = result.toJson();
    assert(json.find("executeTask") != std::string::npos);
    assert(json.find("tickNumber") != std::string::npos);
    printf("  PASS: testTickResultToJson\n");
}

void runTickEngineTests() {
    printf("=== test_tick_engine ===\n");
    testTickExecuteTask();
    testTickDelegate();
    testTickLowEnergyRest();
    testTickCounter();
    testTickResultToJson();
    printf("All 5 tick engine tests PASSED.\n");
}
