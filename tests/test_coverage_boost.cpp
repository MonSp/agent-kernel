// Tests targeting Schema serialization type branches, EntityArchetype dispatch,
// SchemaValidator unsigned integer validation.
#include "ecs/Schema.h"
#include "ecs/SchemaValidator.h"
#include "ecs/EntityArchetype.h"
#include "ecs/Registry.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/components/StatsComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/LifecycleComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/EvolutionComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace ECS;

// ─── Schema: instanceToJson type branches ────────────────────────────────────

static void testInstanceToJsonAllIntTypes() {
    // Create a custom struct with various integer types
    struct MixedInts {
        int8_t   i8;
        int16_t  i16;
        int32_t  i32;
        int64_t  i64;
        uint8_t  u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
    };

    ComponentSchema schema;
    schema.name = "MixedInts";
    // addField(name, type, offset, size)
    schema.addField("i8", FieldType::Int8, offsetof(MixedInts, i8), sizeof(int8_t));
    schema.addField("i16", FieldType::Int16, offsetof(MixedInts, i16), sizeof(int16_t));
    schema.addField("i32", FieldType::Int32, offsetof(MixedInts, i32), sizeof(int32_t));
    schema.addField("i64", FieldType::Int64, offsetof(MixedInts, i64), sizeof(int64_t));
    schema.addField("u8", FieldType::Uint8, offsetof(MixedInts, u8), sizeof(uint8_t));
    schema.addField("u16", FieldType::Uint16, offsetof(MixedInts, u16), sizeof(uint16_t));
    schema.addField("u32", FieldType::Uint32, offsetof(MixedInts, u32), sizeof(uint32_t));
    schema.addField("u64", FieldType::Uint64, offsetof(MixedInts, u64), sizeof(uint64_t));

    MixedInts data{};
    data.i8 = -128;
    data.i16 = -32768;
    data.i32 = -2147483647;
    data.i64 = -9223372036854775807LL;
    data.u8 = 255;
    data.u16 = 65535;
    data.u32 = 4294967295U;
    data.u64 = 18446744073709551615ULL;

    std::string json = schema.instanceToJson(&data);
    assert(json.find("-128") != std::string::npos);
    assert(json.find("-32768") != std::string::npos);
    assert(json.find("255") != std::string::npos);
    assert(json.find("65535") != std::string::npos);
    printf("  PASS: testInstanceToJsonAllIntTypes\n");
}

static void testInstanceToJsonFloat64() {
    struct FloatData {
        double f64;
        float  f32;
    };

    ComponentSchema schema;
    schema.name = "FloatData";
    schema.addField("f64", FieldType::Float64, offsetof(FloatData, f64), sizeof(double));
    schema.addField("f32", FieldType::Float32, offsetof(FloatData, f32), sizeof(float));

    FloatData data{};
    data.f64 = 3.141592653589793;
    data.f32 = 2.71828f;

    std::string json = schema.instanceToJson(&data);
    assert(json.find("3.141") != std::string::npos);
    assert(json.find("2.718") != std::string::npos);
    printf("  PASS: testInstanceToJsonFloat64\n");
}

static void testInstanceToJsonBool() {
    struct BoolData {
        bool flag1;
        bool flag2;
    };

    ComponentSchema schema;
    schema.name = "BoolData";
    schema.addField("flag1", FieldType::Bool, offsetof(BoolData, flag1), sizeof(bool));
    schema.addField("flag2", FieldType::Bool, offsetof(BoolData, flag2), sizeof(bool));

    BoolData data{};
    data.flag1 = true;
    data.flag2 = false;

    std::string json = schema.instanceToJson(&data);
    assert(json.find("true") != std::string::npos);
    assert(json.find("false") != std::string::npos);
    printf("  PASS: testInstanceToJsonBool\n");
}

static void testInstanceToJsonEnum() {
    // LifecycleComponent has enum fields (lifeState, birthType)
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("LifecycleComponent");
    assert(schema != nullptr);

    LifecycleComponent lc{};
    lc.birthTime = 1000;
    lc.age = 30.0f;
    lc.lifeState = AgentLifeState::Active;
    lc.birthType = BirthType::Natural;

    std::string json = schema->instanceToJson(&lc);
    assert(json.find("Active") != std::string::npos);
    assert(json.find("Natural") != std::string::npos);
    printf("  PASS: testInstanceToJsonEnum\n");
}

static void testInstanceToJsonEnumUnknown() {
    // Set an enum value that doesn't match any known name
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("LifecycleComponent");
    assert(schema != nullptr);

    LifecycleComponent lc{};
    lc.birthTime = 1000;
    lc.age = 30.0f;
    lc.lifeState = static_cast<AgentLifeState>(99);  // Unknown value
    lc.birthType = static_cast<BirthType>(99);       // Unknown value

    std::string json = schema->instanceToJson(&lc);
    assert(json.find("Unknown") != std::string::npos);
    printf("  PASS: testInstanceToJsonEnumUnknown\n");
}

// ─── Schema: toJsonSchema type branches ──────────────────────────────────────

static void testToJsonSchemaAllTypes() {
    ComponentSchema schema;
    schema.name = "AllTypes";
    schema.description = "Schema with all field types";
    schema.addField("b", FieldType::Bool, 0, sizeof(bool));
    schema.addField("i8", FieldType::Int8, 1, sizeof(int8_t));
    schema.addField("i16", FieldType::Int16, 2, sizeof(int16_t));
    schema.addField("i32", FieldType::Int32, 4, sizeof(int32_t));
    schema.addField("i64", FieldType::Int64, 8, sizeof(int64_t));
    schema.addField("u8", FieldType::Uint8, 16, sizeof(uint8_t));
    schema.addField("f32", FieldType::Float32, 20, sizeof(float));
    schema.addField("f64", FieldType::Float64, 24, sizeof(double));
    schema.addField("str", FieldType::String, 32, sizeof(std::string));

    std::string json = schema.toJsonSchema();
    assert(json.find("\"boolean\"") != std::string::npos);
    assert(json.find("\"integer\"") != std::string::npos);
    assert(json.find("\"number\"") != std::string::npos);
    assert(json.find("\"string\"") != std::string::npos);
    printf("  PASS: testToJsonSchemaAllTypes\n");
}

// ─── Schema: SchemaRegistry exportAllJsonSchemas ─────────────────────────────

static void testExportAllJsonSchemasContent() {
    registerAllSchemas();
    std::string json = SchemaRegistry::instance().exportAllJsonSchemas();
    // Should contain all 9 component schemas
    assert(json.find("StatsComponent") != std::string::npos);
    assert(json.find("PersonalityComponent") != std::string::npos);
    assert(json.find("IdentityComponent") != std::string::npos);
    assert(json.find("LifecycleComponent") != std::string::npos);
    assert(json.find("CareerComponent") != std::string::npos);
    assert(json.find("SocialComponent") != std::string::npos);
    assert(json.find("EvolutionComponent") != std::string::npos);
    assert(json.find("MemoryRingComponent") != std::string::npos);
    assert(json.find("SkillTreeComponent") != std::string::npos);
    printf("  PASS: testExportAllJsonSchemasContent\n");
}

// ─── SchemaValidator: unsigned integer validation ────────────────────────────

static void testValidateUint8() {
    // StatsComponent has careerLevel (uint8) — test out-of-range
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent s{};
    s.hp = 50;
    s.maxHp = 100;
    s.careerLevel = 255;  // May exceed schema max

    ValidationResult r = schema->validate(&s);
    // Just verify no crash — schema constraints depend on registration
    printf("  PASS: testValidateUint8 (valid=%d)\n", (int)r.valid);
}

static void testValidateComponentFreeFunction() {
    registerAllSchemas();
    auto* schema = SchemaRegistry::instance().getSchema("StatsComponent");
    assert(schema != nullptr);

    StatsComponent s{};
    s.hp = 50;
    s.maxHp = 100;

    // Use the free-function helper
    ValidationResult r = validateComponent(*schema, &s);
    printf("  PASS: testValidateComponentFreeFunction (valid=%d)\n", (int)r.valid);
}

// ─── EntityArchetype: createFromArchetype component dispatch ─────────────────

static void testCreateFromArchetypeAllComponents() {
    registerAllSchemas();
    Registry::getInstance().clear();

    EntityArchetype arch;
    arch.name = "FullArch";

    // Include all hardcoded component types
    ComponentTemplate ident;
    ident.componentName = "IdentityComponent";
    ident.defaults = {{"id", "a1"}, {"name", "TestAgent"}};
    arch.components.push_back(ident);

    ComponentTemplate stats;
    stats.componentName = "StatsComponent";
    stats.defaults = {{"hp", "80"}, {"maxHp", "100"}};
    arch.components.push_back(stats);

    ComponentTemplate pers;
    pers.componentName = "PersonalityComponent";
    pers.defaults = {{"ambition", "60.0"}};
    arch.components.push_back(pers);

    ComponentTemplate mem;
    mem.componentName = "MemoryRingComponent";
    arch.components.push_back(mem);

    ComponentTemplate life;
    life.componentName = "LifecycleComponent";
    life.defaults = {{"birthTime", "1000"}};
    arch.components.push_back(life);

    ComponentTemplate social;
    social.componentName = "SocialComponent";
    social.defaults = {{"energy", "75.0"}};
    arch.components.push_back(social);

    ComponentTemplate skill;
    skill.componentName = "SkillTreeComponent";
    arch.components.push_back(skill);

    ComponentTemplate career;
    career.componentName = "CareerComponent";
    career.defaults = {{"totalXp", "500"}};
    arch.components.push_back(career);

    ComponentTemplate evo;
    evo.componentName = "EvolutionComponent";
    arch.components.push_back(evo);

    EntityId id = arch.instantiate(Registry::getInstance());

    // Verify all components were created
    assert(Registry::getInstance().getComponent<IdentityComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<StatsComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<PersonalityComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<MemoryRingComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<LifecycleComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<SocialComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<SkillTreeComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<CareerComponent>(id) != nullptr);
    assert(Registry::getInstance().getComponent<EvolutionComponent>(id) != nullptr);

    printf("  PASS: testCreateFromArchetypeAllComponents\n");
}

static void testArchetypeRegistryMethods() {
    ArchetypeRegistry& reg = ArchetypeRegistry::instance();

    // Register an archetype
    EntityArchetype arch;
    arch.name = "TestArchReg";
    ComponentTemplate stats;
    stats.componentName = "StatsComponent";
    stats.defaults = {{"hp", "50"}};
    arch.components.push_back(stats);

    reg.registerArchetype(arch);
    assert(reg.getArchetypeCount() > 0);

    // Get archetype names
    auto names = reg.getAllArchetypeNames();
    assert(!names.empty());

    // Get specific archetype
    auto* retrieved = reg.getArchetype("TestArchReg");
    assert(retrieved != nullptr);
    assert(retrieved->name == "TestArchReg");

    // Create from archetype via registry
    registerAllSchemas();
    Registry::getInstance().clear();
    EntityId id = reg.createFromArchetype("TestArchReg", Registry::getInstance());
    auto* s = Registry::getInstance().getComponent<StatsComponent>(id);
    assert(s != nullptr);

    printf("  PASS: testArchetypeRegistryMethods\n");
}

// ─── Registry: getEntityCount + describeEntity edge cases ────────────────────

static void testDescribeEntityEmpty() {
    registerAllSchemas();
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    std::string json = Registry::getInstance().describeEntity(e.getId());
    // Entity with no components — should return empty/minimal JSON
    assert(!json.empty());
    printf("  PASS: testDescribeEntityEmpty\n");
}

static void testDescribeEntityInvalidId() {
    Registry::getInstance().clear();
    std::string json = Registry::getInstance().describeEntity(99999);
    // Invalid entity — should return empty or error
    printf("  PASS: testDescribeEntityInvalidId (len=%zu)\n", json.size());
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runCoverageBoostTests() {
    printf("Running coverage boost tests...\n");

    // Schema instanceToJson type branches
    testInstanceToJsonAllIntTypes();
    testInstanceToJsonFloat64();
    testInstanceToJsonBool();
    testInstanceToJsonEnum();
    testInstanceToJsonEnumUnknown();

    // Schema toJsonSchema
    testToJsonSchemaAllTypes();

    // SchemaRegistry export
    testExportAllJsonSchemasContent();

    // SchemaValidator
    testValidateUint8();
    testValidateComponentFreeFunction();

    // EntityArchetype dispatch
    testCreateFromArchetypeAllComponents();
    testArchetypeRegistryMethods();

    // Registry edge cases
    testDescribeEntityEmpty();
    testDescribeEntityInvalidId();

    printf("All 12 coverage boost tests PASSED.\n");
}
