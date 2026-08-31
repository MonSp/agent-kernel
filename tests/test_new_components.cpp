#include "agent_kernel.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace ECS;

static void testSkillTreeComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& st = reg.addComponent<SkillTreeComponent>(e.getId());

    // Add skills with dependencies
    st.addSkill("cpp_basics", SkillCategory::Engineering, SkillLevel::Beginner);
    st.addSkill("cpp_advanced", SkillCategory::Engineering, SkillLevel::Beginner,
                {"cpp_basics"});
    st.addSkill("system_design", SkillCategory::Engineering, SkillLevel::Beginner,
                {"cpp_advanced"});

    assert(st.hasSkill("cpp_basics"));
    assert(st.hasSkill("cpp_advanced"));
    assert(!st.hasSkill("nonexistent"));

    // Verify skill details
    auto* s = st.getSkill("cpp_advanced");
    assert(s != nullptr);
    assert(s->skillId == "cpp_advanced");
    assert(s->category == SkillCategory::Engineering);
    assert(s->level == SkillLevel::Beginner);
    assert(s->dependencies.size() == 1);
    assert(s->dependencies[0] == "cpp_basics");

    // Add XP and trigger level-up
    st.addXp("cpp_basics", 999);
    assert(st.getSkill("cpp_basics")->level == SkillLevel::Beginner);
    assert(st.getSkill("cpp_basics")->xp == 999);

    st.addXp("cpp_basics", 1);  // hits 1000 -> level up
    assert(st.getSkill("cpp_basics")->level == SkillLevel::Intermediate);
    assert(st.getSkill("cpp_basics")->xp == 0);

    // Add more XP for multiple level-ups
    st.addXp("cpp_basics", 3000);  // xp=3000: Inter->Adv(xp=2000), Adv->Expert(xp=1000)
    assert(st.getSkill("cpp_basics")->level == SkillLevel::Expert);
    assert(st.getSkill("cpp_basics")->xp == 1000);  // remainder stops at Expert cap

    // addXp to non-existent skill returns false
    assert(!st.addXp("nonexistent", 100));

    // Test getOverallLevel
    // cpp_basics=Expert(4), cpp_advanced=Beginner(1), system_design=Beginner(1)
    float overall = st.getOverallLevel();
    float expected = (4.0f + 1.0f + 1.0f) / 3.0f;
    assert(std::fabs(overall - expected) < 0.001f);

    printf("  PASS: testSkillTreeComponent\n");
}

static void testCareerComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& cc = reg.addComponent<CareerComponent>(e.getId());

    assert(cc.stage == CareerStage::Junior);
    assert(cc.totalXp == 0);
    assert(!cc.canPromote());

    // Add XP below threshold
    cc.addXp(499);
    assert(cc.stage == CareerStage::Junior);
    assert(cc.totalXp == 499);
    assert(!cc.canPromote());

    // Cross Junior threshold (>=500) -> auto-promote to Mid
    cc.addXp(1);
    assert(cc.totalXp == 500);
    assert(cc.stage == CareerStage::Mid);

    // Add XP to reach Senior threshold (>=2000)
    cc.addXp(1500);
    assert(cc.totalXp == 2000);
    assert(cc.stage == CareerStage::Senior);

    // Add XP to reach Lead threshold (>=5000)
    cc.addXp(3000);
    assert(cc.totalXp == 5000);
    assert(cc.stage == CareerStage::Lead);

    // Add XP to reach Expert threshold (>=10000)
    cc.addXp(5000);
    assert(cc.totalXp == 10000);
    assert(cc.stage == CareerStage::Expert);

    // Cannot promote beyond Expert
    assert(!cc.canPromote());
    cc.addXp(99999);
    assert(cc.stage == CareerStage::Expert);

    // Test success rate
    cc.tasksCompleted = 10;
    cc.tasksSucceeded = 7;
    assert(std::fabs(cc.getSuccessRate() - 0.7f) < 0.001f);

    // Zero tasks case
    cc.tasksCompleted = 0;
    cc.tasksSucceeded = 0;
    assert(cc.getSuccessRate() == 0.0f);

    printf("  PASS: testCareerComponent\n");
}

static void testEvolutionComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    auto& ec = reg.addComponent<EvolutionComponent>(e.getId());

    assert(ec.totalEvolutions == 0);
    assert(ec.successfulEvolutions == 0);
    assert(ec.getSuccessRate() == 0.0f);

    // Record evolutions
    ec.recordEvolution("rule_A", 0.5f, 0.7f, 1000);  // successful
    assert(ec.totalEvolutions == 1);
    assert(ec.successfulEvolutions == 1);

    ec.recordEvolution("rule_A", 0.8f, 0.6f, 2000);  // failed (regression)
    assert(ec.totalEvolutions == 2);
    assert(ec.successfulEvolutions == 1);
    assert(std::fabs(ec.getSuccessRate() - 0.5f) < 0.001f);

    // Record more for rule_A
    ec.recordEvolution("rule_A", 0.4f, 0.9f, 3000);  // successful, now 3 for rule_A
    assert(ec.totalEvolutions == 3);

    // shouldEvolve: rule_A has 3 evolutions, should return false
    assert(!ec.shouldEvolve("rule_A"));

    // rule_B has 0 evolutions, should return true
    assert(ec.shouldEvolve("rule_B"));

    // Verify history contents
    assert(ec.history.size() == 3);
    assert(ec.history[0].ruleId == "rule_A");
    assert(ec.history[0].effectivenessBefore == 0.5f);
    assert(ec.history[0].effectivenessAfter == 0.7f);
    assert(ec.history[0].timestamp == 1000);

    printf("  PASS: testEvolutionComponent\n");
}

// Called from main() in test_ecs_core.cpp
void runNewComponentTests() {
    printf("Running agent-kernel new component tests...\n");

    testSkillTreeComponent();
    testCareerComponent();
    testEvolutionComponent();

    printf("All 3 new component tests PASSED.\n");
}
