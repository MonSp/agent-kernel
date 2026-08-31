// test_component_schemas.cpp — Tests for registerAllSchemas() and the 9 component schemas

#include "agent_kernel.h"
#include "ecs/Schema.h"
#include "ecs/ComponentSchemas.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace ECS;

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ─── Test: registerAllSchemas registers exactly 9 schemas ─────────────────

static void testAllNineSchemasRegistered() {
    registerAllSchemas();
    auto& reg = SchemaRegistry::instance();

    // Verify all 9 names are present
    const char* expected[] = {
        "IdentityComponent",
        "StatsComponent",
        "PersonalityComponent",
        "LifecycleComponent",
        "SocialComponent",
        "MemoryRingComponent",
        "SkillTreeComponent",
        "CareerComponent",
        "EvolutionComponent"
    };

    for (const char* name : expected) {
        const auto* s = reg.getSchema(name);
        assert(s != nullptr && "Schema must be registered");
        assert(s->name == std::string(name));
    }

    // Verify total count is exactly 9 (the prior test_schema.cpp tests
    // registered StatsComponent and PersonalityComponent which get overwritten)
    assert(reg.getSchemaCount() == 9);

    printf("  PASS: testAllNineSchemasRegistered\n");
}

// ─── Test: field counts per schema ────────────────────────────────────────

static void testFieldCounts() {
    auto& reg = SchemaRegistry::instance();

    // IdentityComponent: 6 fields (id, name, department, companyRole, teamId, role)
    assert(reg.getSchema("IdentityComponent")->getFieldCount() == 6);

    // StatsComponent: 8 fields
    assert(reg.getSchema("StatsComponent")->getFieldCount() == 8);

    // PersonalityComponent: 6 fields
    assert(reg.getSchema("PersonalityComponent")->getFieldCount() == 6);

    // LifecycleComponent: 5 fields (skip optional<DeathCause>)
    assert(reg.getSchema("LifecycleComponent")->getFieldCount() == 5);

    // SocialComponent: 8 float fields
    assert(reg.getSchema("SocialComponent")->getFieldCount() == 8);

    // MemoryRingComponent: 0 fields (name only)
    assert(reg.getSchema("MemoryRingComponent")->getFieldCount() == 0);

    // SkillTreeComponent: 0 fields (name only)
    assert(reg.getSchema("SkillTreeComponent")->getFieldCount() == 0);

    // CareerComponent: 5 fields
    assert(reg.getSchema("CareerComponent")->getFieldCount() == 5);

    // EvolutionComponent: 3 fields (skip vector)
    assert(reg.getSchema("EvolutionComponent")->getFieldCount() == 3);

    printf("  PASS: testFieldCounts\n");
}

// ─── Test: StatsComponent instanceToJson ──────────────────────────────────

static void testStatsInstanceToJson() {
    auto& reg = SchemaRegistry::instance();
    const auto* schema = reg.getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent stats;
    stats.power = 150;
    stats.hp = 50;
    stats.maxHp = 1000;
    stats.mp = 200;
    stats.maxMp = 500;
    stats.realm = RealmLevel::GoldenCore;
    stats.xp = 12500;
    stats.careerLevel = 3;

    std::string json = schema->instanceToJson(&stats);

    assert(contains(json, "\"hp\":50"));
    assert(contains(json, "\"power\":150"));
    assert(contains(json, "\"maxHp\":1000"));
    assert(contains(json, "\"mp\":200"));
    assert(contains(json, "\"maxMp\":500"));
    assert(contains(json, "\"realm\":\"GoldenCore\""));
    assert(contains(json, "\"xp\":12500"));
    assert(contains(json, "\"careerLevel\":3"));

    printf("  PASS: testStatsInstanceToJson\n");
}

// ─── Test: PersonalityComponent constraint verification ───────────────────

static void testPersonalityConstraints() {
    auto& reg = SchemaRegistry::instance();
    const auto* schema = reg.getSchema("PersonalityComponent");
    assert(schema != nullptr);

    // Check that all 6 personality traits have constraint [0, 100]
    const char* traits[] = {"ambition", "caution", "loyalty", "greed", "sociability", "diligence"};
    for (const char* trait : traits) {
        const auto* f = schema->getField(trait);
        assert(f != nullptr);
        assert(f->type == FieldType::Float32);
        assert(f->constraint.min == 0.0f);
        assert(f->constraint.max == 100.0f);
        assert(f->constraint.required == true);
    }

    // Also test instanceToJson works on PersonalityComponent
    PersonalityComponent p;
    p.ambition = 80.5f;
    p.caution = 45.0f;
    p.loyalty = 92.0f;
    p.greed = 30.0f;
    p.sociability = 55.5f;
    p.diligence = 70.0f;

    std::string json = schema->instanceToJson(&p);
    assert(contains(json, "\"ambition\":"));
    assert(contains(json, "\"caution\":"));
    assert(contains(json, "\"loyalty\":"));
    assert(contains(json, "\"greed\":"));
    assert(contains(json, "\"sociability\":"));
    assert(contains(json, "\"diligence\":"));

    printf("  PASS: testPersonalityConstraints\n");
}

// ─── Test: enum fields have populated enumValues ──────────────────────────

static void testEnumFieldValues() {
    auto& reg = SchemaRegistry::instance();

    // IdentityComponent.role
    {
        const auto* schema = reg.getSchema("IdentityComponent");
        const auto* f = schema->getField("role");
        assert(f != nullptr);
        assert(f->type == FieldType::Enum);
        assert(f->enumValues.size() == 5);
        assert(f->enumValues[0].second == "Worker");
        assert(f->enumValues[4].second == "Director");
    }

    // StatsComponent.realm
    {
        const auto* schema = reg.getSchema("StatsComponent");
        const auto* f = schema->getField("realm");
        assert(f != nullptr);
        assert(f->type == FieldType::Enum);
        assert(f->enumValues.size() == 6);
        assert(f->enumValues[0].second == "Mortal");
        assert(f->enumValues[5].second == "Transcension");
    }

    // LifecycleComponent.lifeState
    {
        const auto* schema = reg.getSchema("LifecycleComponent");
        const auto* f = schema->getField("lifeState");
        assert(f != nullptr);
        assert(f->type == FieldType::Enum);
        assert(f->enumValues.size() == 4);
        assert(f->enumValues[0].second == "Idle");
        assert(f->enumValues[3].second == "Terminated");
    }

    // LifecycleComponent.birthType
    {
        const auto* schema = reg.getSchema("LifecycleComponent");
        const auto* f = schema->getField("birthType");
        assert(f != nullptr);
        assert(f->type == FieldType::Enum);
        assert(f->enumValues.size() == 4);
        assert(f->enumValues[0].second == "Natural");
        assert(f->enumValues[3].second == "DemonBeast");
    }

    // CareerComponent.stage
    {
        const auto* schema = reg.getSchema("CareerComponent");
        const auto* f = schema->getField("stage");
        assert(f != nullptr);
        assert(f->type == FieldType::Enum);
        assert(f->enumValues.size() == 5);
        assert(f->enumValues[0].second == "Junior");
        assert(f->enumValues[4].second == "Expert");
    }

    printf("  PASS: testEnumFieldValues\n");
}

// ─── Test: exportAllJsonSchemas produces valid JSON with all 9 names ──────

static void testExportAllJsonSchemas() {
    auto& reg = SchemaRegistry::instance();
    std::string allJson = reg.exportAllJsonSchemas();

    // Must contain all 9 component names as keys
    const char* expected[] = {
        "IdentityComponent",
        "StatsComponent",
        "PersonalityComponent",
        "LifecycleComponent",
        "SocialComponent",
        "MemoryRingComponent",
        "SkillTreeComponent",
        "CareerComponent",
        "EvolutionComponent"
    };
    for (const char* name : expected) {
        assert(contains(allJson, std::string("\"") + name + "\""));
    }

    // Must be valid-ish JSON Schema wrapper
    assert(contains(allJson, "\"type\": \"object\""));
    assert(contains(allJson, "\"properties\""));

    printf("  PASS: testExportAllJsonSchemas\n");
}

// ─── Entry point ──────────────────────────────────────────────────────────

void runComponentSchemaRegistrationTests() {
    printf("Running component schema registration tests...\n");

    testAllNineSchemasRegistered();
    testFieldCounts();
    testStatsInstanceToJson();
    testPersonalityConstraints();
    testEnumFieldValues();
    testExportAllJsonSchemas();

    printf("All 6 component schema registration tests PASSED.\n");
}
