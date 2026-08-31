// test_archetype.cpp — Tests for EntityArchetype and ArchetypeRegistry

#include "agent_kernel.h"
#include "ecs/Schema.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/GenericComponentStore.h"
#include "ecs/EntityArchetype.h"
#include "ecs/BuiltinArchetypes.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace ECS;

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ─── Test: Create EntityArchetype manually, register, and instantiate ────────

static void testManualArchetypeWithDefaults() {
    Registry::getInstance().clear();
    registerAllSchemas();

    // Create a custom archetype with 2 components and defaults
    EntityArchetype arch;
    arch.name = "TestFighter";
    arch.description = "A test fighter archetype";

    arch.components.push_back({"IdentityComponent", {
        {"role", "Worker"},
        {"department", "Battle"},
        {"companyRole", "Fighter"}
    }});
    arch.components.push_back({"StatsComponent", {
        {"hp", "200"},
        {"maxHp", "200"},
        {"mp", "80"},
        {"maxMp", "80"}
    }});

    // Register in ArchetypeRegistry
    auto& archReg = ArchetypeRegistry::instance();
    archReg.registerArchetype(std::move(arch));

    // Verify registration
    const EntityArchetype* retrieved = archReg.getArchetype("TestFighter");
    assert(retrieved != nullptr);
    assert(retrieved->name == "TestFighter");
    assert(retrieved->components.size() == 2);

    // Create entity from archetype
    auto& reg = Registry::getInstance();
    EntityId id = archReg.createFromArchetype("TestFighter", reg);
    assert(reg.isEntityValid(id));

    // Verify IdentityComponent has defaults
    IdentityComponent* identity = reg.getComponent<IdentityComponent>(id);
    assert(identity != nullptr);
    assert(identity->department == "Battle");
    assert(identity->companyRole == "Fighter");
    assert(identity->role == AgentRole::Worker);

    // Verify StatsComponent has defaults
    StatsComponent* stats = reg.getComponent<StatsComponent>(id);
    assert(stats != nullptr);
    assert(stats->hp == 200);
    assert(stats->maxHp == 200);
    assert(stats->mp == 80);
    assert(stats->maxMp == 80);

    printf("  PASS: testManualArchetypeWithDefaults\n");
}

// ─── Test: registerBuiltinArchetypes registers 6 archetypes ──────────────────

static void testBuiltinArchetypesRegistered() {
    Registry::getInstance().clear();

    // Create a fresh ArchetypeRegistry scope — we can't reset the singleton,
    // but registerBuiltinArchetypes has a guard, so we just call it once.
    registerBuiltinArchetypes();

    auto& archReg = ArchetypeRegistry::instance();
    // 6 builtins + possible manual archetypes from prior tests
    assert(archReg.getArchetypeCount() >= 6);

    // Verify the 6 builtin names are all present
    auto names = archReg.getAllArchetypeNames();

    // Check each name exists
    bool hasEngineer = false, hasDesigner = false, hasManager = false;
    bool hasWarrior = false, hasAlchemist = false, hasElder = false;
    for (auto& n : names) {
        if (n == "Engineer") hasEngineer = true;
        if (n == "Designer") hasDesigner = true;
        if (n == "Manager") hasManager = true;
        if (n == "Warrior") hasWarrior = true;
        if (n == "Alchemist") hasAlchemist = true;
        if (n == "Elder") hasElder = true;
    }
    assert(hasEngineer && hasDesigner && hasManager);
    assert(hasWarrior && hasAlchemist && hasElder);

    printf("  PASS: testBuiltinArchetypesRegistered\n");
}

// ─── Test: Create entity from "Warrior" archetype, verify Stats hp=100 ───────

static void testWarriorArchetype() {
    Registry::getInstance().clear();

    auto& archReg = ArchetypeRegistry::instance();
    auto& reg = Registry::getInstance();

    EntityId id = archReg.createFromArchetype("Warrior", reg);
    assert(reg.isEntityValid(id));

    // Stats should have hp=100, maxHp=100, mp=50, maxMp=50
    StatsComponent* stats = reg.getComponent<StatsComponent>(id);
    assert(stats != nullptr);
    assert(stats->hp == 100);
    assert(stats->maxHp == 100);
    assert(stats->mp == 50);
    assert(stats->maxMp == 50);

    // Should have all 6 components
    assert(reg.hasComponent<IdentityComponent>(id));
    assert(reg.hasComponent<StatsComponent>(id));
    assert(reg.hasComponent<PersonalityComponent>(id));
    assert(reg.hasComponent<SkillTreeComponent>(id));
    assert(reg.hasComponent<CareerComponent>(id));
    assert(reg.hasComponent<LifecycleComponent>(id));

    // Personality defaults
    PersonalityComponent* pers = reg.getComponent<PersonalityComponent>(id);
    assert(pers != nullptr);
    assert(pers->ambition == 50.0f);
    assert(pers->diligence == 50.0f);

    // Lifecycle defaults
    LifecycleComponent* life = reg.getComponent<LifecycleComponent>(id);
    assert(life != nullptr);
    assert(life->age == 0.0f);
    assert(life->lifeState == AgentLifeState::Active);

    // Career defaults
    CareerComponent* career = reg.getComponent<CareerComponent>(id);
    assert(career != nullptr);
    assert(career->stage == CareerStage::Junior);
    assert(career->totalXp == 0);

    printf("  PASS: testWarriorArchetype\n");
}

// ─── Test: Create entity from "Engineer" archetype, verify Identity role ─────

static void testEngineerArchetype() {
    Registry::getInstance().clear();

    auto& archReg = ArchetypeRegistry::instance();
    auto& reg = Registry::getInstance();

    EntityId id = archReg.createFromArchetype("Engineer", reg);
    assert(reg.isEntityValid(id));

    // Identity should have role=Specialist, department=Engineering
    IdentityComponent* identity = reg.getComponent<IdentityComponent>(id);
    assert(identity != nullptr);
    assert(identity->role == AgentRole::Specialist);
    assert(identity->department == "Engineering");

    // Should have 4 components: Identity, SkillTree, Career, Evolution
    assert(reg.hasComponent<IdentityComponent>(id));
    assert(reg.hasComponent<SkillTreeComponent>(id));
    assert(reg.hasComponent<CareerComponent>(id));
    assert(reg.hasComponent<EvolutionComponent>(id));

    // Should NOT have Stats or Personality
    assert(!reg.hasComponent<StatsComponent>(id));
    assert(!reg.hasComponent<PersonalityComponent>(id));
    assert(!reg.hasComponent<LifecycleComponent>(id));

    printf("  PASS: testEngineerArchetype\n");
}

// ─── Test: getArchetype and getAllArchetypeNames ─────────────────────────────

static void testGetArchetypeAndNames() {
    auto& archReg = ArchetypeRegistry::instance();

    // getArchetype for known names
    assert(archReg.getArchetype("Engineer") != nullptr);
    assert(archReg.getArchetype("Designer") != nullptr);
    assert(archReg.getArchetype("Manager") != nullptr);
    assert(archReg.getArchetype("Warrior") != nullptr);
    assert(archReg.getArchetype("Alchemist") != nullptr);
    assert(archReg.getArchetype("Elder") != nullptr);

    // getArchetype for unknown name
    assert(archReg.getArchetype("NonExistent") == nullptr);

    // getAllArchetypeNames should return 7 (6 builtin + 1 manual from earlier test)
    auto names = archReg.getAllArchetypeNames();
    assert(names.size() >= 6);  // at least 6 builtins (plus TestFighter from earlier test)

    printf("  PASS: testGetArchetypeAndNames\n");
}

// ─── Test: create from non-existent archetype throws ─────────────────────────

static void testNonExistentArchetypeThrows() {
    Registry::getInstance().clear();

    auto& archReg = ArchetypeRegistry::instance();
    auto& reg = Registry::getInstance();

    bool threw = false;
    try {
        archReg.createFromArchetype("Ghost", reg);
    } catch (const std::runtime_error& e) {
        threw = true;
        assert(contains(e.what(), "Ghost"));
    }
    assert(threw && "Expected runtime_error for non-existent archetype");

    printf("  PASS: testNonExistentArchetypeThrows\n");
}

// ─── Entry point ─────────────────────────────────────────────────────────────

void runArchetypeTests() {
    printf("Running archetype tests...\n");

    testManualArchetypeWithDefaults();
    testBuiltinArchetypesRegistered();
    testWarriorArchetype();
    testEngineerArchetype();
    testGetArchetypeAndNames();
    testNonExistentArchetypeThrows();

    printf("All 6 archetype tests PASSED.\n");
}
