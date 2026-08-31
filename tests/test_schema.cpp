#include "agent_kernel.h"
#include "ecs/Schema.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace ECS;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ─── Test: manual schema creation + field queries ────────────────────────────

static void testManualSchemaCreation() {
    ComponentSchema schema;
    schema.name = "StatsComponent";
    schema.description = "Combat stats for an agent";

    // Use offsetof with a known struct to get real offsets
    schema.addField("power", FieldType::Int32,
                    offsetof(StatsComponent, power), sizeof(int32_t),
                    "int32_t", "Combat power");
    schema.addField("hp", FieldType::Int32,
                    offsetof(StatsComponent, hp), sizeof(int32_t),
                    "int32_t", "Current HP");
    schema.addField("maxHp", FieldType::Int32,
                    offsetof(StatsComponent, maxHp), sizeof(int32_t),
                    "int32_t", "Maximum HP");
    schema.addField("mp", FieldType::Int32,
                    offsetof(StatsComponent, mp), sizeof(int32_t),
                    "int32_t", "Current MP");
    schema.addField("maxMp", FieldType::Int32,
                    offsetof(StatsComponent, maxMp), sizeof(int32_t),
                    "int32_t", "Maximum MP");
    schema.addEnumField("realm", offsetof(StatsComponent, realm), sizeof(RealmLevel),
                        {{0, "Mortal"}, {1, "QiRefining"}, {2, "FoundationBuilding"},
                         {3, "GoldenCore"}, {4, "YuanInfant"}, {5, "Transcension"}},
                        "Cultivation realm");
    schema.addField("xp", FieldType::Int32,
                    offsetof(StatsComponent, xp), sizeof(int32_t),
                    "int32_t", "Experience points");
    schema.addField("careerLevel", FieldType::Uint8,
                    offsetof(StatsComponent, careerLevel), sizeof(uint8_t),
                    "uint8_t", "Career level");

    // Verify
    assert(schema.name == "StatsComponent");
    assert(schema.description == "Combat stats for an agent");
    assert(schema.getFieldCount() == 8);

    const auto* hpField = schema.getField("hp");
    assert(hpField != nullptr);
    assert(hpField->name == "hp");
    assert(hpField->type == FieldType::Int32);
    assert(hpField->offset == offsetof(StatsComponent, hp));
    assert(hpField->size == sizeof(int32_t));
    assert(hpField->typeName == "int32_t");
    assert(hpField->description == "Current HP");

    const auto* realmField = schema.getField("realm");
    assert(realmField != nullptr);
    assert(realmField->type == FieldType::Enum);
    assert(realmField->enumValues.size() == 6);
    assert(realmField->enumValues[0].second == "Mortal");
    assert(realmField->enumValues[5].second == "Transcension");

    const auto* missing = schema.getField("nonexistent");
    assert(missing == nullptr);

    printf("  PASS: testManualSchemaCreation\n");
}

// ─── Test: addFieldWithConstraint ────────────────────────────────────────────

static void testConstrainedFields() {
    ComponentSchema schema;
    schema.name = "PersonalityComponent";
    schema.description = "6-dimension personality model";

    schema.addFieldWithConstraint("ambition", FieldType::Float32,
                                  offsetof(PersonalityComponent, ambition), sizeof(float),
                                  0.0f, 100.0f, "Ambition level");
    schema.addFieldWithConstraint("caution", FieldType::Float32,
                                  offsetof(PersonalityComponent, caution), sizeof(float),
                                  0.0f, 100.0f, "Caution level");
    schema.addFieldWithConstraint("loyalty", FieldType::Float32,
                                  offsetof(PersonalityComponent, loyalty), sizeof(float),
                                  0.0f, 100.0f, "Loyalty level");
    schema.addFieldWithConstraint("greed", FieldType::Float32,
                                  offsetof(PersonalityComponent, greed), sizeof(float),
                                  0.0f, 100.0f, "Greed level");
    schema.addFieldWithConstraint("sociability", FieldType::Float32,
                                  offsetof(PersonalityComponent, sociability), sizeof(float),
                                  0.0f, 100.0f, "Sociability level");
    schema.addFieldWithConstraint("diligence", FieldType::Float32,
                                  offsetof(PersonalityComponent, diligence), sizeof(float),
                                  0.0f, 100.0f, "Diligence level");

    assert(schema.getFieldCount() == 6);

    const auto* amb = schema.getField("ambition");
    assert(amb != nullptr);
    assert(amb->constraint.min == 0.0f);
    assert(amb->constraint.max == 100.0f);
    assert(amb->constraint.required == true);

    printf("  PASS: testConstrainedFields\n");
}

// ─── Test: SchemaRegistry register/query/enumerate ───────────────────────────

static void testSchemaRegistry() {
    auto& reg = SchemaRegistry::instance();

    ComponentSchema stats;
    stats.name = "StatsComponent";
    stats.addField("hp", FieldType::Int32, 0, 4, "int32_t", "HP");
    reg.registerSchema("StatsComponent", stats);

    ComponentSchema personality;
    personality.name = "PersonalityComponent";
    personality.addField("ambition", FieldType::Float32, 0, 4, "float", "Ambition");
    reg.registerSchema("PersonalityComponent", personality);

    assert(reg.getSchemaCount() == 2);

    const auto* s = reg.getSchema("StatsComponent");
    assert(s != nullptr);
    assert(s->name == "StatsComponent");
    assert(s->getFieldCount() == 1);

    const auto* p = reg.getSchema("PersonalityComponent");
    assert(p != nullptr);
    assert(p->name == "PersonalityComponent");

    const auto* missing = reg.getSchema("NonexistentComponent");
    assert(missing == nullptr);

    auto names = reg.getAllSchemaNames();
    assert(names.size() == 2);
    // Names should contain both
    bool hasStats = false, hasPersonality = false;
    for (auto& n : names) {
        if (n == "StatsComponent") hasStats = true;
        if (n == "PersonalityComponent") hasPersonality = true;
    }
    assert(hasStats);
    assert(hasPersonality);

    printf("  PASS: testSchemaRegistry\n");
}

// ─── Test: instanceToJson on a real StatsComponent ───────────────────────────

static void testInstanceToJsonStats() {
    ComponentSchema schema;
    schema.name = "StatsComponent";
    schema.addField("power", FieldType::Int32,
                    offsetof(StatsComponent, power), sizeof(int32_t), "int32_t", "Combat power");
    schema.addField("hp", FieldType::Int32,
                    offsetof(StatsComponent, hp), sizeof(int32_t), "int32_t", "Current HP");
    schema.addField("maxHp", FieldType::Int32,
                    offsetof(StatsComponent, maxHp), sizeof(int32_t), "int32_t", "Maximum HP");
    schema.addField("mp", FieldType::Int32,
                    offsetof(StatsComponent, mp), sizeof(int32_t), "int32_t", "Current MP");
    schema.addField("maxMp", FieldType::Int32,
                    offsetof(StatsComponent, maxMp), sizeof(int32_t), "int32_t", "Maximum MP");
    schema.addEnumField("realm", offsetof(StatsComponent, realm), sizeof(RealmLevel),
                        {{0, "Mortal"}, {1, "QiRefining"}, {2, "FoundationBuilding"},
                         {3, "GoldenCore"}, {4, "YuanInfant"}, {5, "Transcension"}},
                        "Cultivation realm");
    schema.addField("xp", FieldType::Int32,
                    offsetof(StatsComponent, xp), sizeof(int32_t), "int32_t", "XP");
    schema.addField("careerLevel", FieldType::Uint8,
                    offsetof(StatsComponent, careerLevel), sizeof(uint8_t), "uint8_t", "Career level");

    // Create a real StatsComponent instance
    StatsComponent stats;
    stats.power = 150;
    stats.hp = 800;
    stats.maxHp = 1000;
    stats.mp = 300;
    stats.maxMp = 500;
    stats.realm = RealmLevel::GoldenCore;
    stats.xp = 12500;
    stats.careerLevel = 5;

    std::string json = schema.instanceToJson(&stats);

    // Verify the JSON contains expected values
    assert(contains(json, "\"power\":150"));
    assert(contains(json, "\"hp\":800"));
    assert(contains(json, "\"maxHp\":1000"));
    assert(contains(json, "\"mp\":300"));
    assert(contains(json, "\"maxMp\":500"));
    assert(contains(json, "\"realm\":\"GoldenCore\""));
    assert(contains(json, "\"xp\":12500"));
    assert(contains(json, "\"careerLevel\":5"));

    printf("  PASS: testInstanceToJsonStats\n");
}

// ─── Test: instanceToJson on PersonalityComponent ────────────────────────────

static void testInstanceToJsonPersonality() {
    ComponentSchema schema;
    schema.name = "PersonalityComponent";
    schema.addField("ambition", FieldType::Float32,
                    offsetof(PersonalityComponent, ambition), sizeof(float), "float", "Ambition");
    schema.addField("caution", FieldType::Float32,
                    offsetof(PersonalityComponent, caution), sizeof(float), "float", "Caution");
    schema.addField("loyalty", FieldType::Float32,
                    offsetof(PersonalityComponent, loyalty), sizeof(float), "float", "Loyalty");
    schema.addField("greed", FieldType::Float32,
                    offsetof(PersonalityComponent, greed), sizeof(float), "float", "Greed");
    schema.addField("sociability", FieldType::Float32,
                    offsetof(PersonalityComponent, sociability), sizeof(float), "float", "Sociability");
    schema.addField("diligence", FieldType::Float32,
                    offsetof(PersonalityComponent, diligence), sizeof(float), "float", "Diligence");

    PersonalityComponent personality;
    personality.ambition = 80.5f;
    personality.caution = 45.0f;
    personality.loyalty = 92.3f;
    personality.greed = 30.0f;
    personality.sociability = 55.5f;
    personality.diligence = 70.0f;

    std::string json = schema.instanceToJson(&personality);

    // Float values should be present
    assert(contains(json, "\"ambition\":"));
    assert(contains(json, "\"caution\":"));
    assert(contains(json, "\"loyalty\":"));
    assert(contains(json, "\"greed\":"));
    assert(contains(json, "\"sociability\":"));
    assert(contains(json, "\"diligence\":"));

    printf("  PASS: testInstanceToJsonPersonality\n");
}

// ─── Test: toJsonSchema ──────────────────────────────────────────────────────

static void testToJsonSchema() {
    ComponentSchema schema;
    schema.name = "StatsComponent";
    schema.description = "Combat stats";

    schema.addFieldWithConstraint("hp", FieldType::Int32,
                                  offsetof(StatsComponent, hp), sizeof(int32_t),
                                  0, 99999, "Current HP");
    schema.addFieldWithConstraint("maxHp", FieldType::Int32,
                                  offsetof(StatsComponent, maxHp), sizeof(int32_t),
                                  0, 99999, "Maximum HP");
    schema.addEnumField("realm", offsetof(StatsComponent, realm), sizeof(RealmLevel),
                        {{0, "Mortal"}, {1, "QiRefining"}, {2, "FoundationBuilding"},
                         {3, "GoldenCore"}, {4, "YuanInfant"}, {5, "Transcension"}},
                        "Cultivation realm");

    std::string jsonSchema = schema.toJsonSchema();

    // Should be valid-ish JSON Schema structure
    assert(contains(jsonSchema, "\"type\": \"object\""));
    assert(contains(jsonSchema, "\"title\": \"StatsComponent\""));
    assert(contains(jsonSchema, "\"description\": \"Combat stats\""));
    assert(contains(jsonSchema, "\"hp\""));
    assert(contains(jsonSchema, "\"maxHp\""));
    assert(contains(jsonSchema, "\"realm\""));
    assert(contains(jsonSchema, "\"integer\""));
    assert(contains(jsonSchema, "\"minimum\": 0"));
    assert(contains(jsonSchema, "\"maximum\": 99999"));
    assert(contains(jsonSchema, "\"required\""));
    assert(contains(jsonSchema, "\"hp\"")); // required field
    assert(contains(jsonSchema, "\"Mortal\""));
    assert(contains(jsonSchema, "\"Transcension\""));

    printf("  PASS: testToJsonSchema\n");
}

// ─── Test: exportAllJsonSchemas ──────────────────────────────────────────────

static void testExportAllJsonSchemas() {
    auto& reg = SchemaRegistry::instance();

    // Ensure schemas are registered (from prior test or register fresh)
    ComponentSchema s1;
    s1.name = "StatsComponent";
    s1.addField("hp", FieldType::Int32, 0, 4, "int32_t", "HP");
    reg.registerSchema("StatsComponent", s1);

    ComponentSchema s2;
    s2.name = "PersonalityComponent";
    s2.addField("ambition", FieldType::Float32, 0, 4, "float", "Ambition");
    reg.registerSchema("PersonalityComponent", s2);

    std::string allSchemas = reg.exportAllJsonSchemas();

    // Should contain both schema names
    assert(contains(allSchemas, "StatsComponent"));
    assert(contains(allSchemas, "PersonalityComponent"));
    assert(contains(allSchemas, "\"type\": \"object\""));

    printf("  PASS: testExportAllJsonSchemas\n");
}

// ─── Test: Bool field serialization ──────────────────────────────────────────

static void testBoolFieldSerialization() {
    struct MockComponent {
        bool active;
        int32_t value;
    };

    ComponentSchema schema;
    schema.name = "MockComponent";
    schema.addField("active", FieldType::Bool,
                    offsetof(MockComponent, active), sizeof(bool),
                    "bool", "Is active");
    schema.addField("value", FieldType::Int32,
                    offsetof(MockComponent, value), sizeof(int32_t),
                    "int32_t", "Some value");

    MockComponent mc;
    mc.active = true;
    mc.value = 42;

    std::string json = schema.instanceToJson(&mc);
    assert(contains(json, "\"active\":true"));

    mc.active = false;
    json = schema.instanceToJson(&mc);
    assert(contains(json, "\"active\":false"));
    assert(contains(json, "\"value\":42"));

    printf("  PASS: testBoolFieldSerialization\n");
}

// ─── Test: String field serialization ────────────────────────────────────────

static void testStringFieldSerialization() {
    struct MockComponent {
        int32_t id;
        std::string name;
    };

    ComponentSchema schema;
    schema.name = "MockNamedComponent";
    schema.addField("id", FieldType::Int32,
                    offsetof(MockComponent, id), sizeof(int32_t),
                    "int32_t", "ID");
    schema.addField("name", FieldType::String,
                    offsetof(MockComponent, name), sizeof(std::string),
                    "std::string", "Name");

    MockComponent mc;
    mc.id = 7;
    mc.name = "Alice \"The Great\"";

    std::string json = schema.instanceToJson(&mc);
    assert(contains(json, "\"id\":7"));
    assert(contains(json, "\"name\":\"Alice \\\"The Great\\\"\""));

    printf("  PASS: testStringFieldSerialization\n");
}

// ─── Entry point ─────────────────────────────────────────────────────────────

void runSchemaTests() {
    printf("Running agent-kernel schema tests...\n");

    testManualSchemaCreation();
    testConstrainedFields();
    testSchemaRegistry();
    testInstanceToJsonStats();
    testInstanceToJsonPersonality();
    testToJsonSchema();
    testExportAllJsonSchemas();
    testBoolFieldSerialization();
    testStringFieldSerialization();

    printf("All 9 schema tests PASSED.\n");
}
