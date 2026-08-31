// test_hybrid_registry.cpp — Tests for hybrid Registry (hardcoded + dynamic components)

#include "agent_kernel.h"
#include "ecs/Schema.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/GenericComponentStore.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace ECS;

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ─── Custom struct for dynamic component testing ─────────────────────────────

struct CustomData {
    int32_t score;
    float multiplier;
    uint32_t flags;
};

// ─── Test: Hardcoded IdentityComponent still works through Registry ──────────

static void testHardcodedIdentityComponent() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Add hardcoded IdentityComponent
    auto& identity = reg.addComponent<IdentityComponent>(id);
    identity.id = "agent-001";
    identity.name = "TestAgent";
    identity.department = "Engineering";
    identity.companyRole = "Developer";
    identity.teamId = "team-alpha";
    identity.role = AgentRole::Worker;

    // Verify getComponent works
    IdentityComponent* comp = reg.getComponent<IdentityComponent>(id);
    assert(comp != nullptr);
    assert(comp->id == "agent-001");
    assert(comp->name == "TestAgent");
    assert(comp->department == "Engineering");
    assert(comp->role == AgentRole::Worker);

    // Verify hasComponent
    assert(reg.hasComponent<IdentityComponent>(id));
    assert(!reg.hasComponent<StatsComponent>(id));

    printf("  PASS: testHardcodedIdentityComponent\n");
}

// ─── Test: Register and use a new dynamic component type ─────────────────────

static void testDynamicCustomComponent() {
    Registry::getInstance().clear();

    // Register CustomData as a dynamic component type
    ComponentSchema customSchema;
    customSchema.name = "CustomData";
    customSchema.description = "Custom test data component";

    customSchema.addField("score", FieldType::Int32,
                          offsetof(CustomData, score), sizeof(int32_t),
                          "int32_t", "Test score");
    customSchema.addFieldWithConstraint("multiplier", FieldType::Float32,
                                        offsetof(CustomData, multiplier), sizeof(float),
                                        0.0f, 100.0f, "Score multiplier");
    customSchema.addField("flags", FieldType::Uint32,
                          offsetof(CustomData, flags), sizeof(uint32_t),
                          "uint32_t", "Feature flags");

    auto& schemaReg = SchemaRegistry::instance();
    schemaReg.registerSchema("CustomData", std::move(customSchema));

    const ComponentSchema* schemaPtr = schemaReg.getSchema("CustomData");
    assert(schemaPtr != nullptr);

    auto& dynReg = DynamicComponentRegistry::instance();
    dynReg.registerComponent("CustomData", schemaPtr, sizeof(CustomData));
    assert(dynReg.hasComponent("CustomData"));

    // Create entity and set the dynamic component
    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();

    CustomData data;
    std::memset(&data, 0, sizeof(data));
    data.score = 42;
    data.multiplier = 2.5f;
    data.flags = 0xABCD;

    reg.setDynamicComponent(id, "CustomData", &data);

    // Verify getDynamicComponent returns correct data
    void* raw = reg.getDynamicComponent(id, "CustomData");
    assert(raw != nullptr);
    const CustomData* retrieved = static_cast<const CustomData*>(raw);
    assert(retrieved->score == 42);
    assert(retrieved->multiplier == 2.5f);
    assert(retrieved->flags == 0xABCD);

    // Verify hasDynamicComponent
    assert(reg.hasDynamicComponent(id, "CustomData"));
    assert(!reg.hasDynamicComponent(id, "Nonexistent"));

    printf("  PASS: testDynamicCustomComponent\n");
}

// ─── Test: describeEntity returns both hardcoded and dynamic components ───────

static void testDescribeEntityHybrid() {
    Registry::getInstance().clear();

    // Ensure dynamic stores for hardcoded components exist
    initDynamicRegistryFromSchemas();

    // Register CustomData if not already registered
    auto& dynReg = DynamicComponentRegistry::instance();
    if (!dynReg.hasComponent("CustomData")) {
        ComponentSchema customSchema;
        customSchema.name = "CustomData";
        customSchema.addField("score", FieldType::Int32,
                              offsetof(CustomData, score), sizeof(int32_t),
                              "int32_t", "Test score");
        customSchema.addFieldWithConstraint("multiplier", FieldType::Float32,
                                            offsetof(CustomData, multiplier), sizeof(float),
                                            0.0f, 100.0f, "Score multiplier");
        customSchema.addField("flags", FieldType::Uint32,
                              offsetof(CustomData, flags), sizeof(uint32_t),
                              "uint32_t", "Feature flags");

        auto& schemaReg = SchemaRegistry::instance();
        schemaReg.registerSchema("CustomData", std::move(customSchema));
        dynReg.registerComponent("CustomData", schemaReg.getSchema("CustomData"), sizeof(CustomData));
    }

    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Set hardcoded IdentityComponent
    auto& identity = reg.addComponent<IdentityComponent>(id);
    identity.id = "agent-hybrid";
    identity.name = "HybridAgent";
    identity.department = "R&D";
    identity.companyRole = "Researcher";
    identity.teamId = "team-beta";
    identity.role = AgentRole::Specialist;

    // Set hardcoded StatsComponent
    auto& stats = reg.addComponent<StatsComponent>(id);
    stats.hp = 80;
    stats.maxHp = 100;
    stats.power = 150;

    // Set dynamic CustomData
    CustomData data;
    std::memset(&data, 0, sizeof(data));
    data.score = 99;
    data.multiplier = 3.14f;
    data.flags = 0x1234;
    reg.setDynamicComponent(id, "CustomData", &data);

    // Describe entity
    std::string desc = reg.describeEntity(id);

    // Should contain all three components
    assert(contains(desc, "\"entityId\":"));
    assert(contains(desc, "\"IdentityComponent\":"));
    assert(contains(desc, "\"StatsComponent\":"));
    assert(contains(desc, "\"CustomData\":"));

    // Should contain actual field values
    assert(contains(desc, "agent-hybrid"));
    assert(contains(desc, "\"hp\":80"));
    assert(contains(desc, "\"score\":99"));

    printf("  PASS: testDescribeEntityHybrid\n");
}

// ─── Test: removeDynamicComponent works ──────────────────────────────────────

static void testRemoveDynamicComponent() {
    Registry::getInstance().clear();

    // Ensure CustomData is registered
    auto& dynReg = DynamicComponentRegistry::instance();
    if (!dynReg.hasComponent("CustomData")) {
        ComponentSchema customSchema;
        customSchema.name = "CustomData";
        customSchema.addField("score", FieldType::Int32,
                              offsetof(CustomData, score), sizeof(int32_t),
                              "int32_t", "Test score");
        auto& schemaReg = SchemaRegistry::instance();
        schemaReg.registerSchema("CustomData", std::move(customSchema));
        dynReg.registerComponent("CustomData", schemaReg.getSchema("CustomData"), sizeof(CustomData));
    }

    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();

    CustomData data;
    std::memset(&data, 0, sizeof(data));
    data.score = 77;
    reg.setDynamicComponent(id, "CustomData", &data);
    assert(reg.hasDynamicComponent(id, "CustomData"));

    // Remove it
    reg.removeDynamicComponent(id, "CustomData");
    assert(!reg.hasDynamicComponent(id, "CustomData"));
    assert(reg.getDynamicComponent(id, "CustomData") == nullptr);

    printf("  PASS: testRemoveDynamicComponent\n");
}

// ─── Test: dynamicComponentToJson ────────────────────────────────────────────

static void testDynamicComponentToJson() {
    Registry::getInstance().clear();

    // Ensure CustomData is registered
    auto& dynReg = DynamicComponentRegistry::instance();
    if (!dynReg.hasComponent("CustomData")) {
        ComponentSchema customSchema;
        customSchema.name = "CustomData";
        customSchema.addField("score", FieldType::Int32,
                              offsetof(CustomData, score), sizeof(int32_t),
                              "int32_t", "Test score");
        customSchema.addFieldWithConstraint("multiplier", FieldType::Float32,
                                            offsetof(CustomData, multiplier), sizeof(float),
                                            0.0f, 100.0f, "Score multiplier");
        auto& schemaReg = SchemaRegistry::instance();
        schemaReg.registerSchema("CustomData", std::move(customSchema));
        dynReg.registerComponent("CustomData", schemaReg.getSchema("CustomData"), sizeof(CustomData));
    }

    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();

    CustomData data;
    std::memset(&data, 0, sizeof(data));
    data.score = 55;
    data.multiplier = 1.5f;
    reg.setDynamicComponent(id, "CustomData", &data);

    std::string json = reg.dynamicComponentToJson(id, "CustomData");
    assert(contains(json, "\"score\":55"));
    assert(contains(json, "1.5"));

    // For nonexistent component
    std::string empty = reg.dynamicComponentToJson(id, "Nonexistent");
    assert(empty == "{}");

    printf("  PASS: testDynamicComponentToJson\n");
}

// ─── Test: destroyEntity cleans up dynamic components ────────────────────────

static void testDestroyEntityCleansDynamic() {
    Registry::getInstance().clear();

    // Ensure CustomData is registered
    auto& dynReg = DynamicComponentRegistry::instance();
    if (!dynReg.hasComponent("CustomData")) {
        ComponentSchema customSchema;
        customSchema.name = "CustomData";
        customSchema.addField("score", FieldType::Int32,
                              offsetof(CustomData, score), sizeof(int32_t),
                              "int32_t", "Test score");
        auto& schemaReg = SchemaRegistry::instance();
        schemaReg.registerSchema("CustomData", std::move(customSchema));
        dynReg.registerComponent("CustomData", schemaReg.getSchema("CustomData"), sizeof(CustomData));
    }

    auto& reg = Registry::getInstance();
    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Set both hardcoded and dynamic components
    reg.addComponent<IdentityComponent>(id).id = "to-destroy";

    CustomData data;
    std::memset(&data, 0, sizeof(data));
    data.score = 100;
    reg.setDynamicComponent(id, "CustomData", &data);
    assert(reg.hasDynamicComponent(id, "CustomData"));

    // Destroy entity — should clean up dynamic components too
    reg.destroyEntity(id);

    // Entity should be gone
    assert(!reg.isEntityValid(id));
    assert(reg.getDynamicComponent(id, "CustomData") == nullptr);

    printf("  PASS: testDestroyEntityCleansDynamic\n");
}

// ─── Test: describeEntity on nonexistent entity ──────────────────────────────

static void testDescribeEntityInvalid() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    std::string desc = reg.describeEntity(99999);
    assert(desc == "{}");

    // Create and destroy entity, then try describe
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.destroyEntity(id);
    desc = reg.describeEntity(id);
    assert(desc == "{}");

    printf("  PASS: testDescribeEntityInvalid\n");
}

// ─── Test: dynamic methods on nonexistent entity ─────────────────────────────

static void testDynamicMethodsInvalidEntity() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    // All methods should gracefully handle invalid entity IDs
    CustomData data;
    std::memset(&data, 0, sizeof(data));
    reg.setDynamicComponent(99999, "CustomData", &data);  // should not crash
    assert(reg.getDynamicComponent(99999, "CustomData") == nullptr);
    assert(!reg.hasDynamicComponent(99999, "CustomData"));
    reg.removeDynamicComponent(99999, "CustomData");  // should not crash
    assert(reg.dynamicComponentToJson(99999, "CustomData") == "{}");

    printf("  PASS: testDynamicMethodsInvalidEntity\n");
}

// ─── Test: existing 46+ tests still pass (no regression marker) ──────────────
// This test just verifies the Registry basic operations work after modifications.

static void testNoRegressionBasicOperations() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    // Create multiple entities
    Entity e1 = reg.createEntity();
    Entity e2 = reg.createEntity();
    Entity e3 = reg.createEntity();
    assert(reg.getEntityCount() == 3);

    // Add components to each
    reg.addComponent<IdentityComponent>(e1.getId()).id = "e1";
    reg.addComponent<StatsComponent>(e2.getId()).hp = 100;
    reg.addComponent<CareerComponent>(e3.getId()).totalXp = 500;

    // Verify
    assert(reg.getComponent<IdentityComponent>(e1.getId())->id == "e1");
    assert(reg.getComponent<StatsComponent>(e2.getId())->hp == 100);
    assert(reg.getComponent<CareerComponent>(e3.getId())->totalXp == 500);

    // Destroy middle entity
    reg.destroyEntity(e2.getId());
    assert(reg.getEntityCount() == 2);
    assert(!reg.isEntityValid(e2.getId()));

    // Others still valid
    assert(reg.isEntityValid(e1.getId()));
    assert(reg.isEntityValid(e3.getId()));

    // Clear all
    reg.clear();
    assert(reg.getEntityCount() == 0);

    printf("  PASS: testNoRegressionBasicOperations\n");
}

// ─── Entry point ─────────────────────────────────────────────────────────────

void runHybridRegistryTests() {
    printf("Running hybrid registry tests...\n");

    testHardcodedIdentityComponent();
    testDynamicCustomComponent();
    testDescribeEntityHybrid();
    testRemoveDynamicComponent();
    testDynamicComponentToJson();
    testDestroyEntityCleansDynamic();
    testDescribeEntityInvalid();
    testDynamicMethodsInvalidEntity();
    testNoRegressionBasicOperations();

    printf("All 9 hybrid registry tests PASSED.\n");
}
