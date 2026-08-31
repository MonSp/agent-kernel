// test_validation.cpp — Tests for schema-driven data validation

#include "agent_kernel.h"
#include "ecs/Schema.h"
#include "ecs/SchemaValidator.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/Registry.h"
#include "ipc/AgentKernelBridge.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace ECS;

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ─── Test: StatsComponent with valid hp ───────────────────────────────────

static void testStatsValidHp() {
    registerAllSchemas();
    auto& schemaReg = SchemaRegistry::instance();
    const auto* schema = schemaReg.getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent stats;
    stats.power = 100;
    stats.hp = 50;
    stats.maxHp = 100;
    stats.mp = 30;
    stats.maxMp = 50;
    stats.realm = RealmLevel::Mortal;
    stats.xp = 0;
    stats.careerLevel = 0;

    ValidationResult result = schema->validate(&stats);
    assert(result.valid == true);
    assert(result.violations.empty());

    printf("  PASS: testStatsValidHp\n");
}

// ─── Test: StatsComponent with hp=-10 → invalid ──────────────────────────

static void testStatsInvalidHp() {
    auto& schemaReg = SchemaRegistry::instance();
    const auto* schema = schemaReg.getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent stats;
    stats.power = 100;
    stats.hp = -10;
    stats.maxHp = 100;
    stats.mp = 30;
    stats.maxMp = 50;
    stats.realm = RealmLevel::Mortal;
    stats.xp = 0;
    stats.careerLevel = 0;

    ValidationResult result = schema->validate(&stats);
    assert(result.valid == false);
    assert(!result.violations.empty());

    // Should have a violation on "hp"
    bool foundHp = false;
    for (const auto& v : result.violations) {
        if (v.fieldName == "hp") {
            foundHp = true;
            assert(contains(v.constraint, "min"));
            assert(contains(v.message, "below minimum"));
            break;
        }
    }
    assert(foundHp && "Must have violation on hp field");

    printf("  PASS: testStatsInvalidHp\n");
}

// ─── Test: PersonalityComponent ambition=150 → invalid (max=100) ──────────

static void testPersonalityAmbitionOverMax() {
    auto& schemaReg = SchemaRegistry::instance();
    const auto* schema = schemaReg.getSchema("PersonalityComponent");
    assert(schema != nullptr);

    PersonalityComponent p;
    p.ambition = 150.0f;
    p.caution = 50.0f;
    p.loyalty = 50.0f;
    p.greed = 50.0f;
    p.sociability = 50.0f;
    p.diligence = 50.0f;

    ValidationResult result = schema->validate(&p);
    assert(result.valid == false);

    bool foundAmbition = false;
    for (const auto& v : result.violations) {
        if (v.fieldName == "ambition") {
            foundAmbition = true;
            assert(contains(v.constraint, "max"));
            assert(contains(v.message, "above maximum"));
            break;
        }
    }
    assert(foundAmbition && "Must have violation on ambition field");

    printf("  PASS: testPersonalityAmbitionOverMax\n");
}

// ─── Test: PersonalityComponent all defaults valid ────────────────────────

static void testPersonalityDefaultsValid() {
    auto& schemaReg = SchemaRegistry::instance();
    const auto* schema = schemaReg.getSchema("PersonalityComponent");
    assert(schema != nullptr);

    PersonalityComponent p; // all defaults are 50.0f

    ValidationResult result = schema->validate(&p);
    assert(result.valid == true);
    assert(result.violations.empty());

    printf("  PASS: testPersonalityDefaultsValid\n");
}

// ─── Test: Registry::validateEntity with defaults → valid ────────────────

static void testValidateEntityDefaultsValid() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity entity = reg.createEntity();
    EntityId eid = entity.getId();

    reg.addComponent<StatsComponent>(eid);
    reg.addComponent<PersonalityComponent>(eid);
    reg.addComponent<LifecycleComponent>(eid);
    reg.addComponent<SocialComponent>(eid);
    reg.addComponent<MemoryRingComponent>(eid);
    reg.addComponent<SkillTreeComponent>(eid);
    reg.addComponent<CareerComponent>(eid);
    reg.addComponent<EvolutionComponent>(eid);

    ValidationResult result = reg.validateEntity(eid);
    assert(result.valid == true);
    assert(result.violations.empty());

    printf("  PASS: testValidateEntityDefaultsValid\n");
}

// ─── Test: Registry::validateEntity detects violations ───────────────────

static void testValidateEntityDetectsViolation() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity entity = reg.createEntity();
    EntityId eid = entity.getId();

    reg.addComponent<StatsComponent>(eid);
    reg.addComponent<PersonalityComponent>(eid);

    // Set invalid values
    auto* stats = reg.getComponent<StatsComponent>(eid);
    stats->hp = -10;

    auto* personality = reg.getComponent<PersonalityComponent>(eid);
    personality->ambition = 150.0f;

    ValidationResult result = reg.validateEntity(eid);
    assert(result.valid == false);

    // Should have violations for StatsComponent.hp and PersonalityComponent.ambition
    bool foundHp = false;
    bool foundAmbition = false;
    for (const auto& v : result.violations) {
        if (contains(v.fieldName, "StatsComponent") && contains(v.fieldName, "hp")) {
            foundHp = true;
        }
        if (contains(v.fieldName, "PersonalityComponent") && contains(v.fieldName, "ambition")) {
            foundAmbition = true;
        }
    }
    assert(foundHp && "Must detect StatsComponent.hp violation");
    assert(foundAmbition && "Must detect PersonalityComponent.ambition violation");

    printf("  PASS: testValidateEntityDetectsViolation\n");
}

// ─── Test: ValidationResult::toJson() ────────────────────────────────────

static void testValidationResultToJson() {
    auto& schemaReg = SchemaRegistry::instance();
    const auto* schema = schemaReg.getSchema("PersonalityComponent");
    assert(schema != nullptr);

    PersonalityComponent p;
    p.ambition = 150.0f;
    p.caution = 50.0f;
    p.loyalty = 50.0f;
    p.greed = -5.0f;
    p.sociability = 50.0f;
    p.diligence = 50.0f;

    ValidationResult result = schema->validate(&p);
    std::string json = result.toJson();

    assert(contains(json, "\"valid\":false"));
    assert(contains(json, "\"violations\""));
    assert(contains(json, "\"fieldName\""));
    assert(contains(json, "\"constraint\""));
    assert(contains(json, "\"actualValue\""));
    assert(contains(json, "\"message\""));
    assert(contains(json, "ambition"));
    assert(contains(json, "greed"));

    // Also test a valid result JSON
    PersonalityComponent p2; // defaults
    ValidationResult result2 = schema->validate(&p2);
    std::string json2 = result2.toJson();
    assert(contains(json2, "\"valid\":true"));
    assert(contains(json2, "\"violations\":[]"));

    printf("  PASS: testValidationResultToJson\n");
}

// ─── Test: IPC validateEntity endpoint ───────────────────────────────────

static void testIpcValidateEntityEndpoint() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity entity = reg.createEntity();
    EntityId eid = entity.getId();

    reg.addComponent<IdentityComponent>(eid, std::string("agent-test"), std::string("TestAgent"),
                                        std::string("Engineering"), std::string("developer"),
                                        std::string("team-1"), AgentRole::Worker);
    reg.addComponent<StatsComponent>(eid);
    reg.addComponent<PersonalityComponent>(eid);
    reg.addComponent<LifecycleComponent>(eid);
    reg.addComponent<SocialComponent>(eid);
    reg.addComponent<MemoryRingComponent>(eid);
    reg.addComponent<SkillTreeComponent>(eid);
    reg.addComponent<CareerComponent>(eid);
    reg.addComponent<EvolutionComponent>(eid);

    // Test via IPC bridge (indirectly — we construct the request JSON)
    std::string request = "{\"method\":\"validateEntity\",\"params\":{\"entityId\":"
                          + std::to_string(eid) + "}}";

    // Create bridge and test the handler directly
    IPC::AgentKernelBridge bridge("/tmp/test_validation.sock");
    // We can't call handleRequest directly (it's private), so we test the
    // Registry::validateEntity + toJson pipeline which is what the handler uses.

    ValidationResult vr = reg.validateEntity(eid);
    std::string response = vr.toJson();

    // All defaults should be valid
    assert(contains(response, "\"valid\":true"));
    assert(contains(response, "\"violations\":[]"));

    // Now make one invalid and re-test
    auto* stats = reg.getComponent<StatsComponent>(eid);
    stats->hp = -10;

    vr = reg.validateEntity(eid);
    response = vr.toJson();
    assert(contains(response, "\"valid\":false"));
    assert(contains(response, "hp"));

    printf("  PASS: testIpcValidateEntityEndpoint\n");
}

// ─── Test: Validate entity not found ─────────────────────────────────────

static void testValidateEntityNotFound() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    ValidationResult result = reg.validateEntity(999);
    assert(result.valid == false);
    assert(!result.violations.empty());
    assert(contains(result.violations[0].fieldName, "entity"));

    printf("  PASS: testValidateEntityNotFound\n");
}

// ─── Entry point ──────────────────────────────────────────────────────────

void runValidationTests() {
    printf("Running validation tests...\n");

    testStatsValidHp();
    testStatsInvalidHp();
    testPersonalityAmbitionOverMax();
    testPersonalityDefaultsValid();
    testValidateEntityDefaultsValid();
    testValidateEntityDetectsViolation();
    testValidationResultToJson();
    testIpcValidateEntityEndpoint();
    testValidateEntityNotFound();

    printf("All 9 validation tests PASSED.\n");
}
