// Tests for ActionTypes — action type enum + Decision mapping.
#include "ecs/systems/ActionTypes.h"
#include "ecs/Registry.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/SocialComponent.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace Systems;
using namespace ECS;

static EntityId createEntityWithSocial(float energy) {
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), "Test", AgentRole::Specialist);
    auto& social = reg.addComponent<SocialComponent>(id);
    social.energy = energy;
    return id;
}

static void testActionTypeRoundtrip() {
    ActionType types[] = {
        ActionType::ExecuteTask, ActionType::PracticeSkill,
        ActionType::Delegate, ActionType::Rest,
        ActionType::Socialize, ActionType::Study, ActionType::Reflect
    };
    for (auto t : types) {
        std::string s = actionTypeToString(t);
        ActionType back = actionTypeFromString(s);
        assert(back == t);
    }
    printf("  PASS: testActionTypeRoundtrip\n");
}

static void testActionTypeFromStringUnknown() {
    assert(actionTypeFromString("unknown") == ActionType::ExecuteTask);
    assert(actionTypeFromString("") == ActionType::ExecuteTask);
    printf("  PASS: testActionTypeFromStringUnknown\n");
}

static void testMapDelegateAction() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(80.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Delegate;
    d.delegateTo = "frontend-team";
    assert(mapDecisionToAction(d, reg, id) == ActionType::Delegate);
    printf("  PASS: testMapDelegateAction\n");
}

static void testMapReflectAction() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(80.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Reflect;
    d.reasoning = "Need to think about this";
    assert(mapDecisionToAction(d, reg, id) == ActionType::Reflect);
    printf("  PASS: testMapReflectAction\n");
}

static void testMapReflectStudyKeywords() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(80.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Reflect;
    d.details = "I should study the architecture";
    assert(mapDecisionToAction(d, reg, id) == ActionType::Study);
    printf("  PASS: testMapReflectStudyKeywords\n");
}

static void testMapLowEnergyRest() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(20.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Execute;
    d.reasoning = "I'll work on this";
    assert(mapDecisionToAction(d, reg, id) == ActionType::Rest);
    printf("  PASS: testMapLowEnergyRest\n");
}

static void testMapSkillPractice() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(80.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Execute;
    d.details = "practice my programming skills";
    assert(mapDecisionToAction(d, reg, id) == ActionType::PracticeSkill);
    printf("  PASS: testMapSkillPractice\n");
}

static void testMapSocialAction() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(80.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Execute;
    d.details = "collaborate with team members";
    assert(mapDecisionToAction(d, reg, id) == ActionType::Socialize);
    printf("  PASS: testMapSocialAction\n");
}

static void testMapDefaultExecuteTask() {
    Registry::getInstance().clear();
    EntityId id = createEntityWithSocial(80.0f);
    auto& reg = Registry::getInstance();
    LLM::Decision d;
    d.action = LLM::Action::Execute;
    d.reasoning = "Implement the feature";
    assert(mapDecisionToAction(d, reg, id) == ActionType::ExecuteTask);
    printf("  PASS: testMapDefaultExecuteTask\n");
}

void runActionTypesTests() {
    printf("=== test_action_types ===\n");
    testActionTypeRoundtrip();
    testActionTypeFromStringUnknown();
    testMapDelegateAction();
    testMapReflectAction();
    testMapReflectStudyKeywords();
    testMapLowEnergyRest();
    testMapSkillPractice();
    testMapSocialAction();
    testMapDefaultExecuteTask();
    printf("All 9 action type tests PASSED.\n");
}
