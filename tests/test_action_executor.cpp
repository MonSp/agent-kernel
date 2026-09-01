// Tests for ActionExecutor — applying effects to ECS entities.
#include "ecs/systems/ActionExecutor.h"
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

static EntityId createFullEntity() {
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), "Test", AgentRole::Specialist);
    auto& social = reg.addComponent<SocialComponent>(id);
    social.energy = 50.0f;
    social.mood = 60.0f;
    social.socialDesire = 40.0f;
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    reg.addComponent<CareerComponent>(id);
    reg.addComponent<PersonalityComponent>(id, 50, 50, 50, 50, 50, 50);
    reg.addComponent<MemoryRingComponent>(id);
    return id;
}

static void testApplySocialEnergy() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::Social, "energy", 30.0f, "", "rest"}};
    ActionExecutor::apply(reg, id, effs);
    auto* social = reg.getComponent<SocialComponent>(id);
    assert(std::abs(social->energy - 80.0f) < 0.01f);
    printf("  PASS: testApplySocialEnergy\n");
}

static void testApplySocialClamping() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::Social, "energy", -80.0f, "", "drain"}};
    ActionExecutor::apply(reg, id, effs);
    auto* social = reg.getComponent<SocialComponent>(id);
    assert(std::abs(social->energy - 0.0f) < 0.01f);
    printf("  PASS: testApplySocialClamping\n");
}

static void testApplySkillTreeXp() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::SkillTree, "backend_dev", 200.0f, "", "task"}};
    ActionExecutor::apply(reg, id, effs);
    auto* skills = reg.getComponent<SkillTreeComponent>(id);
    assert(skills->getSkill("backend_dev")->xp == 200);
    printf("  PASS: testApplySkillTreeXp\n");
}

static void testApplySkillTreeAutoCreate() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::SkillTree, "new_skill", 50.0f, "", "learn"}};
    ActionExecutor::apply(reg, id, effs);
    auto* skills = reg.getComponent<SkillTreeComponent>(id);
    assert(skills->hasSkill("new_skill"));
    assert(skills->getSkill("new_skill")->xp == 50);
    printf("  PASS: testApplySkillTreeAutoCreate\n");
}

static void testApplyCareerXp() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::Career, "totalXp", 100.0f, "", "work"}};
    ActionExecutor::apply(reg, id, effs);
    auto* career = reg.getComponent<CareerComponent>(id);
    assert(career->totalXp == 100);
    printf("  PASS: testApplyCareerXp\n");
}

static void testApplyPersonalityDrift() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::Personality, "sociability", 5.0f, "", "social"}};
    ActionExecutor::apply(reg, id, effs);
    auto* personality = reg.getComponent<PersonalityComponent>(id);
    assert(std::abs(personality->sociability - 55.0f) < 0.01f);
    printf("  PASS: testApplyPersonalityDrift\n");
}

static void testApplyPersonalityClamping() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::Personality, "ambition", 80.0f, "", "boost"}};
    ActionExecutor::apply(reg, id, effs);
    auto* personality = reg.getComponent<PersonalityComponent>(id);
    assert(std::abs(personality->ambition - 100.0f) < 0.01f);
    printf("  PASS: testApplyPersonalityClamping\n");
}

static void testApplyMemoryEntry() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {{TargetComponent::Memory, "milestone", 0.0f, "did something", "record"}};
    ActionExecutor::apply(reg, id, effs);
    auto* memory = reg.getComponent<MemoryRingComponent>(id);
    assert(memory->longTerm.size() == 1);
    printf("  PASS: testApplyMemoryEntry\n");
}

static void testApplyMultipleEffects() {
    Registry::getInstance().clear();
    EntityId id = createFullEntity();
    auto& reg = Registry::getInstance();
    std::vector<ActionEffect> effs = {
        {TargetComponent::Social, "energy", -10.0f, "", "work"},
        {TargetComponent::SkillTree, "backend_dev", 100.0f, "", "task"},
        {TargetComponent::Career, "totalXp", 30.0f, "", "progress"},
        {TargetComponent::Memory, "milestone", 0.0f, "completed task", "record"}
    };
    ActionExecutor::apply(reg, id, effs);
    auto* social = reg.getComponent<SocialComponent>(id);
    auto* skills = reg.getComponent<SkillTreeComponent>(id);
    auto* career = reg.getComponent<CareerComponent>(id);
    auto* memory = reg.getComponent<MemoryRingComponent>(id);
    assert(std::abs(social->energy - 40.0f) < 0.01f);
    assert(skills->getSkill("backend_dev")->xp == 100);
    assert(career->totalXp == 30);
    assert(memory->longTerm.size() == 1);
    printf("  PASS: testApplyMultipleEffects\n");
}

void runActionExecutorTests() {
    printf("=== test_action_executor ===\n");
    testApplySocialEnergy();
    testApplySocialClamping();
    testApplySkillTreeXp();
    testApplySkillTreeAutoCreate();
    testApplyCareerXp();
    testApplyPersonalityDrift();
    testApplyPersonalityClamping();
    testApplyMemoryEntry();
    testApplyMultipleEffects();
    printf("All 9 action executor tests PASSED.\n");
}
