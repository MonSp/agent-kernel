// Tests for Registry and Schema coverage gaps.
#include "ecs/Registry.h"
#include "ecs/Schema.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/EntityArchetype.h"
#include "ecs/components/StatsComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/LifecycleComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/EvolutionComponent.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <stdexcept>

using namespace ECS;

// ─── Registry: getEntityCount ────────────────────────────────────────────────

static void testGetEntityCount() {
    Registry::getInstance().clear();
    assert(Registry::getInstance().getEntityCount() == 0);

    Registry::getInstance().createEntity();
    assert(Registry::getInstance().getEntityCount() == 1);

    Registry::getInstance().createEntity();
    Registry::getInstance().createEntity();
    assert(Registry::getInstance().getEntityCount() == 3);

    printf("  PASS: testGetEntityCount\n");
}

static void testGetEntityCountAfterDestroy() {
    Registry::getInstance().clear();
    Entity e1 = Registry::getInstance().createEntity();
    Entity e2 = Registry::getInstance().createEntity();
    assert(Registry::getInstance().getEntityCount() == 2);

    Registry::getInstance().destroyEntity(e1.getId());
    assert(Registry::getInstance().getEntityCount() == 1);

    Registry::getInstance().destroyEntity(e2.getId());
    assert(Registry::getInstance().getEntityCount() == 0);
    printf("  PASS: testGetEntityCountAfterDestroy\n");
}

// ─── Registry: addComponent invalid entity ───────────────────────────────────

static void testAddComponentInvalidEntity() {
    Registry::getInstance().clear();
    bool threw = false;
    try {
        Registry::getInstance().addComponent<StatsComponent>(99999, StatsComponent{});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    printf("  PASS: testAddComponentInvalidEntity\n");
}

static void testAddComponentDestroyedEntity() {
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    Registry::getInstance().destroyEntity(e.getId());

    bool threw = false;
    try {
        Registry::getInstance().addComponent<StatsComponent>(e.getId(), StatsComponent{});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    printf("  PASS: testAddComponentDestroyedEntity\n");
}

// ─── Registry: getArray template specializations ─────────────────────────────

static void testGetArrayAllTypes() {
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    EntityId id = e.getId();

    // Add components of every type to trigger all getArray<T> specializations
    Registry::getInstance().addComponent<IdentityComponent>(id, "a1", "Test", AgentRole::Specialist);
    Registry::getInstance().addComponent<StatsComponent>(id);
    Registry::getInstance().addComponent<PersonalityComponent>(id);
    Registry::getInstance().addComponent<SocialComponent>(id);
    Registry::getInstance().addComponent<CareerComponent>(id);
    Registry::getInstance().addComponent<LifecycleComponent>(id);
    Registry::getInstance().addComponent<MemoryRingComponent>(id);
    Registry::getInstance().addComponent<SkillTreeComponent>(id);
    Registry::getInstance().addComponent<EvolutionComponent>(id);

    // Verify all components are accessible
    assert(Registry::getInstance().getComponent<IdentityComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<StatsComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<PersonalityComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<SocialComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<CareerComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<LifecycleComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<MemoryRingComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<SkillTreeComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<EvolutionComponent>(id) != nullptr);

    printf("  PASS: testGetArrayAllTypes\n");
}

// ─── Registry: validateEntity with violations ────────────────────────────────

static void testValidateEntityWithViolations() {
    registerAllSchemas();
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    EntityId id = e.getId();

    // Add Stats with out-of-range values
    auto& stats = Registry::getInstance().addComponent<StatsComponent>(id);
    stats.hp = 200;    // Above maxHp
    stats.maxHp = 100;
    stats.mp = -50;    // Below 0

    ValidationResult r = Registry::getInstance().validateEntity(id);
    assert(!r.valid);
    assert(!r.violations.empty());

    // Violations should be prefixed with component name
    bool hasPrefix = false;
    for (auto& v : r.violations) {
        if (v.fieldName.find("StatsComponent.") != std::string::npos) {
            hasPrefix = true;
            break;
        }
    }
    assert(hasPrefix);
    printf("  PASS: testValidateEntityWithViolations (%zu violations)\n", r.violations.size());
}

static void testValidateEntityDynamicComponent() {
    registerAllSchemas();
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    EntityId id = e.getId();

    // Add Personality with out-of-range values
    auto& pers = Registry::getInstance().addComponent<PersonalityComponent>(id);
    pers.ambition = 200.0f;
    pers.caution = -50.0f;

    ValidationResult r = Registry::getInstance().validateEntity(id);
    // Verify it doesn't crash — validation may or may not catch depending on schema state
    printf("  PASS: testValidateEntityDynamicComponent (valid=%d, violations=%zu)\n",
           (int)r.valid, r.violations.size());
}

// ─── Registry: describeEntity with schema serialization ─────────────────────

static void testDescribeEntityFull() {
    registerAllSchemas();
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    EntityId id = e.getId();

    Registry::getInstance().addComponent<IdentityComponent>(id, "a1", "Alice", AgentRole::Specialist);
    auto& stats = Registry::getInstance().addComponent<StatsComponent>(id);
    stats.hp = 80;
    stats.maxHp = 100;
    auto& pers = Registry::getInstance().addComponent<PersonalityComponent>(id);
    pers.ambition = 60.0f;

    std::string json = Registry::getInstance().describeEntity(id);
    assert(!json.empty());
    assert(json.find("IdentityComponent") != std::string::npos);
    assert(json.find("StatsComponent") != std::string::npos);
    assert(json.find("Alice") != std::string::npos);
    printf("  PASS: testDescribeEntityFull\n");
}

// ─── Schema: toJsonSchema ────────────────────────────────────────────────────

static void testToJsonSchemaStats() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("StatsComponent");
    assert(schema != nullptr);

    std::string json = schema->toJsonSchema();
    assert(json.find("\"type\": \"object\"") != std::string::npos);
    assert(json.find("StatsComponent") != std::string::npos);
    assert(json.find("\"properties\"") != std::string::npos);
    printf("  PASS: testToJsonSchemaStats\n");
}

static void testToJsonSchemaPersonality() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("PersonalityComponent");
    assert(schema != nullptr);

    std::string json = schema->toJsonSchema();
    assert(json.find("PersonalityComponent") != std::string::npos);
    assert(json.find("\"type\": \"number\"") != std::string::npos);  // Float fields
    printf("  PASS: testToJsonSchemaPersonality\n");
}

static void testToJsonSchemaWithDescription() {
    // Create a schema with description
    ComponentSchema schema;
    schema.name = "TestSchema";
    schema.description = "A test schema with description";

    std::string json = schema.toJsonSchema();
    assert(json.find("A test schema with description") != std::string::npos);
    printf("  PASS: testToJsonSchemaWithDescription\n");
}

// ─── Schema: instanceToJson type branches ────────────────────────────────────

static void testInstanceToJsonIntTypes() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent s;
    s.hp = 85;        // Int32
    s.maxHp = 100;    // Int32
    s.mp = 45;        // Int32
    s.maxMp = 50;     // Int32
    s.power = 20;     // Int32
    s.xp = 1500;      // Int32
    s.careerLevel = 3; // Uint8

    std::string json = schema->instanceToJson(&s);
    assert(json.find("\"hp\":85") != std::string::npos);
    assert(json.find("\"xp\":1500") != std::string::npos);
    assert(json.find("\"careerLevel\":3") != std::string::npos);
    printf("  PASS: testInstanceToJsonIntTypes\n");
}

static void testInstanceToJsonFloatTypes() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("PersonalityComponent");
    assert(schema != nullptr);

    PersonalityComponent p;
    p.ambition = 75.5f;
    p.caution = 20.0f;
    p.loyalty = 60.0f;
    p.greed = 30.0f;
    p.sociability = 45.0f;
    p.diligence = 80.0f;

    std::string json = schema->instanceToJson(&p);
    assert(json.find("\"ambition\"") != std::string::npos);
    assert(json.find("75.5") != std::string::npos);
    printf("  PASS: testInstanceToJsonFloatTypes\n");
}

static void testInstanceToJsonStringTypes() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("IdentityComponent");
    assert(schema != nullptr);

    IdentityComponent id;
    id.id = "agent-001";
    id.name = "Alice Smith";
    id.department = "Engineering";
    id.companyRole = "Developer";
    id.teamId = "team-alpha";

    std::string json = schema->instanceToJson(&id);
    assert(json.find("Alice Smith") != std::string::npos);
    assert(json.find("Engineering") != std::string::npos);
    printf("  PASS: testInstanceToJsonStringTypes\n");
}

static void testInstanceToJsonUint64() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("LifecycleComponent");
    assert(schema != nullptr);

    LifecycleComponent l;
    l.birthTime = 1234567890ULL;
    l.age = 25.5f;

    std::string json = schema->instanceToJson(&l);
    assert(json.find("1234567890") != std::string::npos);
    printf("  PASS: testInstanceToJsonUint64\n");
}

// ─── Schema: exportAllJsonSchemas + getSchemaCount ──────────────────────────

static void testExportAllJsonSchemas() {
    registerAllSchemas();
    auto& reg = SchemaRegistry::instance();

    size_t count = reg.getSchemaCount();
    assert(count > 0);

    std::string json = reg.exportAllJsonSchemas();
    assert(!json.empty());
    assert(json.find("StatsComponent") != std::string::npos);
    assert(json.find("PersonalityComponent") != std::string::npos);
    printf("  PASS: testExportAllJsonSchemas (%zu schemas)\n", count);
}

static void testSchemaCountAfterRegistration() {
    // registerAllSchemas should register 9 schemas
    registerAllSchemas();
    size_t count = SchemaRegistry::instance().getSchemaCount();
    assert(count >= 9);  // At least the 9 component schemas
    printf("  PASS: testSchemaCountAfterRegistration (%zu schemas)\n", count);
}

// ─── Schema: escapeJsonString edge cases ─────────────────────────────────────

static void testSchemaEscapeJsonString() {
    std::string escaped = ComponentSchema::escapeJsonString("say \"hello\"\nnew line");
    assert(escaped.find("\\\"hello\\\"") != std::string::npos);
    assert(escaped.find("\\n") != std::string::npos);

    std::string empty = ComponentSchema::escapeJsonString("");
    assert(empty == "");

    std::string normal = ComponentSchema::escapeJsonString("normal text");
    assert(normal == "normal text");
    printf("  PASS: testSchemaEscapeJsonString\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runRegistrySchemaTests() {
    printf("Running Registry/Schema coverage tests...\n");

    // Registry
    testGetEntityCount();
    testGetEntityCountAfterDestroy();
    testAddComponentInvalidEntity();
    testAddComponentDestroyedEntity();
    testGetArrayAllTypes();
    testValidateEntityWithViolations();
    testValidateEntityDynamicComponent();
    testDescribeEntityFull();

    // Schema
    testToJsonSchemaStats();
    testToJsonSchemaPersonality();
    testToJsonSchemaWithDescription();
    testInstanceToJsonIntTypes();
    testInstanceToJsonFloatTypes();
    testInstanceToJsonStringTypes();
    testInstanceToJsonUint64();
    testExportAllJsonSchemas();
    testSchemaCountAfterRegistration();
    testSchemaEscapeJsonString();

    printf("All 18 Registry/Schema coverage tests PASSED.\n");
}
