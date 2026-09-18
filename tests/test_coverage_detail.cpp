// Tests for EntityArchetype applyDefaults type branches,
// SocialComponent cooldown, SchemaValidator field-type validation.
#include "ecs/EntityArchetype.h"
#include "ecs/SchemaValidator.h"
#include "ecs/Schema.h"
#include "ecs/Registry.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/components/StatsComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/LifecycleComponent.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace ECS;

// ─── EntityArchetype applyDefaults: type branches ────────────────────────────

static void testApplyDefaultsInt32() {
    registerAllSchemas();
    EntityArchetype arch;
    arch.name = "Int32Arch";
    ComponentTemplate t;
    t.componentName = "StatsComponent";
    t.defaults = {{"hp", "80"}, {"maxHp", "100"}, {"power", "25"}, {"xp", "500"}};
    arch.components.push_back(t);

    Registry::getInstance().clear();
    EntityId id = arch.instantiate(Registry::getInstance());
    auto* s = Registry::getInstance().getComponent<StatsComponent>(id);
    assert(s && s->hp == 80 && s->maxHp == 100 && s->power == 25 && s->xp == 500);
    printf("  PASS: testApplyDefaultsInt32\n");
}

static void testApplyDefaultsUint8() {
    registerAllSchemas();
    EntityArchetype arch;
    arch.name = "Uint8Arch";
    ComponentTemplate t;
    t.componentName = "StatsComponent";
    // careerLevel is uint8
    t.defaults = {{"careerLevel", "3"}};
    arch.components.push_back(t);

    Registry::getInstance().clear();
    EntityId id = arch.instantiate(Registry::getInstance());
    auto* s = Registry::getInstance().getComponent<StatsComponent>(id);
    assert(s && s->careerLevel == 3);
    printf("  PASS: testApplyDefaultsUint8\n");
}

static void testApplyDefaultsFloat() {
    registerAllSchemas();
    EntityArchetype arch;
    arch.name = "FloatArch";
    ComponentTemplate t;
    t.componentName = "PersonalityComponent";
    t.defaults = {{"ambition", "75.5"}, {"caution", "20.0"}};
    arch.components.push_back(t);

    Registry::getInstance().clear();
    EntityId id = arch.instantiate(Registry::getInstance());
    auto* p = Registry::getInstance().getComponent<PersonalityComponent>(id);
    assert(p && p->ambition > 75.0f && p->ambition < 76.0f);
    assert(p && p->caution > 19.0f && p->caution < 21.0f);
    printf("  PASS: testApplyDefaultsFloat\n");
}

static void testApplyDefaultsUint32() {
    registerAllSchemas();
    EntityArchetype arch;
    arch.name = "Uint32Arch";
    ComponentTemplate t;
    t.componentName = "CareerComponent";
    // totalXp is uint32
    t.defaults = {{"totalXp", "1000"}};
    arch.components.push_back(t);

    Registry::getInstance().clear();
    EntityId id = arch.instantiate(Registry::getInstance());
    auto* c = Registry::getInstance().getComponent<CareerComponent>(id);
    assert(c && c->totalXp == 1000);
    printf("  PASS: testApplyDefaultsUint32\n");
}

static void testApplyDefaultsInt64() {
    registerAllSchemas();
    EntityArchetype arch;
    arch.name = "Int64Arch";
    ComponentTemplate t;
    t.componentName = "LifecycleComponent";
    // birthTime is uint64
    t.defaults = {{"birthTime", "12345678"}};
    arch.components.push_back(t);

    Registry::getInstance().clear();
    EntityId id = arch.instantiate(Registry::getInstance());
    auto* l = Registry::getInstance().getComponent<LifecycleComponent>(id);
    assert(l && l->birthTime == 12345678);
    printf("  PASS: testApplyDefaultsInt64\n");
}

static void testApplyDefaultsMixedTypes() {
    registerAllSchemas();
    EntityArchetype arch;
    arch.name = "MixedArch";

    ComponentTemplate statsT;
    statsT.componentName = "StatsComponent";
    statsT.defaults = {{"hp", "60"}, {"maxHp", "100"}, {"careerLevel", "2"}};
    arch.components.push_back(statsT);

    ComponentTemplate persT;
    persT.componentName = "PersonalityComponent";
    persT.defaults = {{"ambition", "80.0"}, {"loyalty", "40.0"}};
    arch.components.push_back(persT);

    Registry::getInstance().clear();
    EntityId id = arch.instantiate(Registry::getInstance());
    auto* s = Registry::getInstance().getComponent<StatsComponent>(id);
    auto* p = Registry::getInstance().getComponent<PersonalityComponent>(id);
    assert(s && s->hp == 60 && s->careerLevel == 2);
    assert(p && p->ambition > 79.0f && p->loyalty < 41.0f);
    printf("  PASS: testApplyDefaultsMixedTypes\n");
}

// ─── SocialComponent: cooldown system ────────────────────────────────────────

static void testCooldownAddAndFill() {
    SocialComponent social;
    social.cooldownCount = 0;

    // Fill all cooldown slots
    for (int i = 0; i < 5; ++i) {
        social.addCooldown(i + 1, EmotionType::Joy, 1, 100 + i);
    }
    assert(social.cooldownCount == 5);

    // Check first entry
    assert(social.emotionCooldowns[0].targetSlot == 1);
    assert(social.emotionCooldowns[0].cooldownUntilFrame == 172);  // 100 + 72
    printf("  PASS: testCooldownAddAndFill\n");
}

static void testCooldownReuseExpiredSlot() {
    SocialComponent social;
    social.cooldownCount = 0;

    // Add one cooldown
    social.addCooldown(1, EmotionType::Joy, 1, 100);
    assert(social.cooldownCount == 1);

    // Add another after first expires (frame 100+72=172)
    social.addCooldown(2, EmotionType::Anger, 2, 200);
    // Should reuse the expired slot
    assert(social.cooldownCount == 1);
    assert(social.emotionCooldowns[0].targetSlot == 2);
    printf("  PASS: testCooldownReuseExpiredSlot\n");
}

static void testCooldownLRUEviction() {
    SocialComponent social;
    social.cooldownCount = 0;

    // Fill all slots with different expiry frames
    for (int i = 0; i < 5; ++i) {
        social.addCooldown(i + 1, EmotionType::Joy, 1, 100 + i * 10);
    }
    assert(social.cooldownCount == 5);

    // Add one more with no expired slots — should trigger LRU eviction
    social.addCooldown(99, EmotionType::Fear, 3, 500);
    assert(social.cooldownCount == 5);  // Still 5, LRU evicted oldest
    // The entry with lowest cooldownUntilFrame (100+72=172) should be replaced
    assert(social.emotionCooldowns[0].targetSlot == 99);
    printf("  PASS: testCooldownLRUEviction\n");
}

// ─── SchemaValidator: field-type validation branches ─────────────────────────

static void testValidateFloatMinMax() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("PersonalityComponent");
    assert(schema != nullptr);

    // PersonalityComponent fields are float with min/max constraints
    PersonalityComponent p;
    p.ambition = 150.0f;  // Above max (100)
    p.caution = -10.0f;   // Below min (0)

    ValidationResult r = schema->validate(&p);
    assert(!r.valid);
    // Should have violations for both ambition (max) and caution (min)
    bool hasMaxViolation = false, hasMinViolation = false;
    for (auto& v : r.violations) {
        if (v.fieldName == "ambition" && v.constraint.find("max") != std::string::npos) hasMaxViolation = true;
        if (v.fieldName == "caution" && v.constraint.find("min") != std::string::npos) hasMinViolation = true;
    }
    assert(hasMaxViolation);
    assert(hasMinViolation);
    printf("  PASS: testValidateFloatMinMax\n");
}

static void testValidateInt32MinMax() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent s;
    s.hp = -50;       // Below min (0)
    s.maxHp = 99999;  // Above max (likely capped)
    s.power = -100;   // Below min

    ValidationResult r = schema->validate(&s);
    assert(!r.valid);
    printf("  PASS: testValidateInt32MinMax\n");
}

static void testValidateValidFloat() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("PersonalityComponent");
    assert(schema != nullptr);

    PersonalityComponent p;
    p.ambition = 50.0f;
    p.caution = 30.0f;
    p.loyalty = 70.0f;
    p.greed = 20.0f;
    p.sociability = 60.0f;
    p.diligence = 80.0f;

    ValidationResult r = schema->validate(&p);
    assert(r.valid);
    printf("  PASS: testValidateValidFloat\n");
}

static void testValidateMultipleViolations() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("PersonalityComponent");
    assert(schema != nullptr);

    PersonalityComponent p;
    p.ambition = 200.0f;   // Above max
    p.caution = -50.0f;    // Below min
    p.loyalty = 300.0f;    // Above max
    p.greed = -200.0f;     // Below min
    p.sociability = 50.0f; // Valid
    p.diligence = 60.0f;   // Valid

    ValidationResult r = schema->validate(&p);
    assert(!r.valid);
    assert(r.violations.size() == 4);  // 4 violations
    printf("  PASS: testValidateMultipleViolations (%zu violations)\n", r.violations.size());
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runCoverageDetailTests() {
    printf("Running coverage detail tests (EntityArchetype/Social/SchemaValidator)...\n");

    // EntityArchetype applyDefaults type branches
    testApplyDefaultsInt32();
    testApplyDefaultsUint8();
    testApplyDefaultsFloat();
    testApplyDefaultsUint32();
    testApplyDefaultsInt64();
    testApplyDefaultsMixedTypes();

    // SocialComponent cooldown
    testCooldownAddAndFill();
    testCooldownReuseExpiredSlot();
    testCooldownLRUEviction();

    // SchemaValidator field-type validation
    testValidateFloatMinMax();
    testValidateInt32MinMax();
    testValidateValidFloat();
    testValidateMultipleViolations();

    printf("All 13 coverage detail tests PASSED.\n");
}
