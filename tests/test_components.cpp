#include "agent_kernel.h"
#include <cassert>
#include <cstdio>

using namespace ECS;

static void testIdentityComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& id = reg.addComponent<IdentityComponent>(e.getId(), "agent-001", "Alice", AgentRole::Worker);

    assert(id.id == "agent-001");
    assert(id.name == "Alice");
    assert(id.role == AgentRole::Worker);
    assert(!id.isLeader());

    id.department = "Engineering";
    id.companyRole = "Senior Developer";
    id.teamId = "team-alpha";
    assert(id.department == "Engineering");
    assert(id.companyRole == "Senior Developer");
    assert(id.teamId == "team-alpha");

    // Test isLeader
    id.role = AgentRole::Lead;
    assert(id.isLeader());
    id.role = AgentRole::Manager;
    assert(id.isLeader());
    id.role = AgentRole::Director;
    assert(id.isLeader());
    id.role = AgentRole::Specialist;
    assert(!id.isLeader());

    printf("  PASS: testIdentityComponent\n");
}

static void testPersonalityComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& p = reg.addComponent<PersonalityComponent>(e.getId(),
        80.0f, 40.0f, 85.0f, 60.0f, 50.0f, 70.0f);

    assert(p.isAggressive());   // ambition 80 > 70 && greed 60 > 50
    assert(!p.isCautious());    // caution 40 <= 70
    assert(p.isLoyal());        // loyalty 85 > 70
    assert(p.isDiligent());     // diligence 70 > 60

    float overall = p.getOverall();
    assert(overall > 60.0f && overall < 70.0f);

    // Test default personality
    Entity e2 = reg.createEntity();
    auto& p2 = reg.addComponent<PersonalityComponent>(e2.getId());
    assert(!p2.isAggressive());
    assert(!p2.isCautious());
    assert(!p2.isLoyal());

    printf("  PASS: testPersonalityComponent\n");
}

static void testStatsComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& s = reg.addComponent<StatsComponent>(e.getId(), 100, 500, 200, RealmLevel::Mortal);

    assert(s.hp == 500);
    assert(s.maxHp == 500);
    assert(s.mp == 200);
    assert(s.power == 100);
    assert(s.xp == 0);
    assert(s.careerLevel == 0);

    s.takeDamage(150);
    assert(s.hp == 350);
    assert(!s.isDead());

    s.heal(50);
    assert(s.hp == 400);

    s.takeDamage(500);
    assert(s.hp == 0);
    assert(s.isDead());

    s.heal(1000);
    assert(s.hp == 500); // clamped to maxHp

    // Test XP field
    s.xp = 1500;
    s.careerLevel = 3;
    assert(s.xp == 1500);
    assert(s.careerLevel == 3);

    assert(s.hpPercent() == 1.0f);
    assert(s.mpPercent() == 1.0f);

    printf("  PASS: testStatsComponent\n");
}

static void testLifecycleComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& lc = reg.addComponent<LifecycleComponent>(e.getId());

    assert(lc.lifeState == AgentLifeState::Active);
    assert(!lc.isTerminated());
    assert(!lc.deathCause.has_value());

    lc.ageOneYear();
    assert(lc.age == 1.0f);

    lc.setDead(DeathCause::Battle);
    assert(lc.lifeState == AgentLifeState::Terminated);
    assert(lc.isTerminated());
    assert(lc.deathCause.has_value());
    assert(lc.deathCause.value() == DeathCause::Battle);

    // Test age-based death check
    Entity e2 = reg.createEntity();
    auto& lc2 = reg.addComponent<LifecycleComponent>(e2.getId());
    lc2.age = 99.0f;
    assert(!lc2.shouldBeDeadByAge(100));
    lc2.age = 100.0f;
    assert(lc2.shouldBeDeadByAge(100));

    printf("  PASS: testLifecycleComponent\n");
}

static void testSocialComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& sc = reg.addComponent<SocialComponent>(e.getId());

    assert(sc.hunger == 0.0f);
    assert(sc.fatigue == 0.0f);
    assert(sc.energy == 80.0f);
    assert(sc.mood == 60.0f);

    sc.tickDaily(1.0f);
    assert(sc.hunger > 0.0f);
    assert(sc.fatigue > 0.0f);
    assert(sc.energy < 80.0f);

    // Test hunger threshold
    sc.hunger = 75.0f;
    assert(sc.isHungry());

    sc.fatigue = 85.0f;
    assert(sc.isExhausted());

    // Test cooldown with generic triggerType
    sc.addCooldown(5, EmotionType::Anger, 3, 100);
    assert(sc.isInCooldown(5, EmotionType::Anger, 3, 150));
    assert(!sc.isInCooldown(5, EmotionType::Anger, 3, 200)); // expired (100+72=172 < 200? no, 172 < 200 = true)
    assert(!sc.isInCooldown(6, EmotionType::Anger, 3, 150)); // different target

    printf("  PASS: testSocialComponent\n");
}

static void testMemoryRingComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& mc = reg.addComponent<MemoryRingComponent>(e.getId());

    assert(mc.interactions.empty());

    // Push an interaction
    InteractionSlot slot;
    slot.timestamp = 1000;
    slot.otherSlot = 42;
    slot.type = 1;
    slot.impactScore = 50;
    mc.interactions.push(slot);

    assert(mc.interactions.size() == 1);

    // Push more and verify count
    for (int i = 0; i < 15; i++) {
        InteractionSlot s;
        s.timestamp = 1000 + i;
        s.otherSlot = i;
        s.type = 0;
        s.impactScore = i;
        mc.interactions.push(s);
    }
    assert(mc.interactions.size() == 16);

    // Push enough to trigger compaction ring
    for (int i = 0; i < 10; i++) {
        InteractionSlot s;
        s.timestamp = 2000 + i;
        s.otherSlot = i + 100;
        s.type = 0;
        s.impactScore = 10;
        mc.interactions.push(s);
    }
    assert(mc.interactions.size() == 20); // at MAX_RECENT_INTERACTIONS

    printf("  PASS: testMemoryRingComponent\n");
}

// Called from main() in test_ecs_core.cpp
void runComponentTests() {
    printf("Running agent-kernel component tests...\n");

    testIdentityComponent();
    testPersonalityComponent();
    testStatsComponent();
    testLifecycleComponent();
    testSocialComponent();
    testMemoryRingComponent();

    printf("All 6 component tests PASSED.\n");
}
