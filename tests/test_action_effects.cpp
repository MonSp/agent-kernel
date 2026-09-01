// Tests for ActionEffect — effect generation for all ActionTypes.
#include "ecs/systems/ActionEffect.h"
#include "ecs/Registry.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace Systems;
using namespace ECS;

static EntityId createFullEntity() {
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), "Test", AgentRole::Specialist);
    reg.addComponent<SocialComponent>(id);
    reg.addComponent<PersonalityComponent>(id, 50, 50, 50, 50, 50, 50);
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    reg.addComponent<CareerComponent>(id);
    reg.addComponent<MemoryRingComponent>(id);
    return id;
}

static LLM::Decision makeDecision(LLM::Action action, const std::string& details = "",
                                   const std::string& delegateTo = "") {
    LLM::Decision d;
    d.action = action;
    d.confidence = 0.8f;
    d.reasoning = "test reasoning";
    d.details = details;
    d.delegateTo = delegateTo;
    return d;
}

static void testExecuteTaskEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::ExecuteTask, makeDecision(LLM::Action::Execute), reg, id);
    assert(effects.size() == 3);
    assert(effects[0].target == TargetComponent::SkillTree);
    assert(effects[0].fieldName == "backend_dev");
    assert(effects[0].delta > 0.0f);
    assert(effects[1].target == TargetComponent::Career);
    assert(effects[2].target == TargetComponent::Memory);
    printf("  PASS: testExecuteTaskEffects\n");
}

static void testPracticeSkillEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::PracticeSkill, makeDecision(LLM::Action::Execute), reg, id);
    assert(effects.size() == 2);
    assert(effects[0].target == TargetComponent::SkillTree);
    assert(effects[0].delta == 100.0f);
    assert(effects[1].target == TargetComponent::Social);
    assert(effects[1].fieldName == "energy");
    assert(effects[1].delta == -10.0f);
    printf("  PASS: testPracticeSkillEffects\n");
}

static void testDelegateEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::Delegate, makeDecision(LLM::Action::Delegate, "", "frontend"), reg, id);
    assert(effects.size() == 1);
    assert(effects[0].target == TargetComponent::Memory);
    assert(effects[0].stringValue.find("frontend") != std::string::npos);
    printf("  PASS: testDelegateEffects\n");
}

static void testRestEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::Rest, makeDecision(LLM::Action::Execute), reg, id);
    assert(effects.size() == 2);
    assert(effects[0].fieldName == "energy" && effects[0].delta == 30.0f);
    assert(effects[1].fieldName == "mood" && effects[1].delta == 10.0f);
    printf("  PASS: testRestEffects\n");
}

static void testSocializeEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::Socialize, makeDecision(LLM::Action::Execute), reg, id);
    assert(effects.size() == 3);
    assert(effects[0].fieldName == "socialDesire" && effects[0].delta == -25.0f);
    assert(effects[1].fieldName == "mood" && effects[1].delta == 15.0f);
    assert(effects[2].fieldName == "sociability" && effects[2].delta == 2.0f);
    printf("  PASS: testSocializeEffects\n");
}

static void testStudyEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::Study, makeDecision(LLM::Action::Reflect), reg, id);
    assert(effects.size() == 2);
    assert(effects[0].target == TargetComponent::SkillTree);
    assert(effects[0].delta == 80.0f);
    assert(effects[1].fieldName == "energy" && effects[1].delta == -15.0f);
    printf("  PASS: testStudyEffects\n");
}

static void testReflectEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    auto effects = generateEffects(ActionType::Reflect, makeDecision(LLM::Action::Reflect), reg, id);
    assert(effects.size() == 2);
    assert(effects[0].target == TargetComponent::Personality);
    assert(std::abs(effects[0].delta) == 3.0f);
    assert(effects[1].target == TargetComponent::Memory);
    printf("  PASS: testReflectEffects\n");
}

static void testExecuteTaskConfidenceScaling() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    LLM::Decision dLow = makeDecision(LLM::Action::Execute);
    dLow.confidence = 0.1f;
    LLM::Decision dHigh = makeDecision(LLM::Action::Execute);
    dHigh.confidence = 0.9f;
    auto effLow = generateEffects(ActionType::ExecuteTask, dLow, reg, id);
    auto effHigh = generateEffects(ActionType::ExecuteTask, dHigh, reg, id);
    assert(effHigh[0].delta > effLow[0].delta);
    printf("  PASS: testExecuteTaskConfidenceScaling\n");
}

void runActionEffectTests() {
    printf("=== test_action_effects ===\n");
    std::srand(42);
    testExecuteTaskEffects();
    testPracticeSkillEffects();
    testDelegateEffects();
    testRestEffects();
    testSocializeEffects();
    testStudyEffects();
    testReflectEffects();
    testExecuteTaskConfidenceScaling();
    printf("All 8 action effect tests PASSED.\n");
}
