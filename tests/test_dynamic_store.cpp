// test_dynamic_store.cpp — Tests for GenericComponentStore, DynamicComponentRegistry,
//                         and initDynamicRegistryFromSchemas()

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

// ─── Test: GenericComponentStore basic set/get/has/remove ─────────────────────

static void testGenericStoreBasicCrud() {
    // Build a schema for StatsComponent manually
    ComponentSchema schema;
    schema.name = "StatsComponent";
    schema.addField("power", FieldType::Int32,
                    offsetof(StatsComponent, power), sizeof(int32_t), "int32_t", "Combat power");
    schema.addFieldWithConstraint("hp", FieldType::Int32,
                                  offsetof(StatsComponent, hp), sizeof(int32_t),
                                  0.0f, 2147483647.0f, "Current HP");
    schema.addFieldWithConstraint("maxHp", FieldType::Int32,
                                  offsetof(StatsComponent, maxHp), sizeof(int32_t),
                                  0.0f, 2147483647.0f, "Maximum HP");
    schema.addFieldWithConstraint("mp", FieldType::Int32,
                                  offsetof(StatsComponent, mp), sizeof(int32_t),
                                  0.0f, 2147483647.0f, "Current MP");
    schema.addFieldWithConstraint("maxMp", FieldType::Int32,
                                  offsetof(StatsComponent, maxMp), sizeof(int32_t),
                                  0.0f, 2147483647.0f, "Maximum MP");
    schema.addEnumField("realm", offsetof(StatsComponent, realm), sizeof(RealmLevel),
                        {{0, "Mortal"}, {1, "QiRefining"}, {2, "FoundationBuilding"},
                         {3, "GoldenCore"}, {4, "YuanInfant"}, {5, "Transcension"}},
                        "Cultivation realm");
    schema.addField("xp", FieldType::Int32,
                    offsetof(StatsComponent, xp), sizeof(int32_t), "int32_t", "Experience points");
    schema.addField("careerLevel", FieldType::Uint8,
                    offsetof(StatsComponent, careerLevel), sizeof(uint8_t), "uint8_t", "Career level");

    // Create store
    GenericComponentStore store(&schema, sizeof(StatsComponent));

    // Initially no data
    assert(!store.has(0));
    assert(store.get(0) == nullptr);
    assert(store.slotCount() == 0);

    // Create a StatsComponent with hp=50
    StatsComponent stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.hp = 50;
    stats.maxHp = 100;
    stats.mp = 30;
    stats.maxMp = 60;
    stats.power = 200;
    stats.xp = 1000;
    stats.realm = RealmLevel::QiRefining;
    stats.careerLevel = 2;

    // Store at slot 0
    store.set(0, &stats);

    // Verify has(0) returns true
    assert(store.has(0));
    assert(store.slotCount() >= 1);

    // Verify get(0) returns correct data
    void* raw = store.get(0);
    assert(raw != nullptr);
    const StatsComponent* retrieved = static_cast<const StatsComponent*>(raw);
    assert(retrieved->hp == 50);
    assert(retrieved->maxHp == 100);
    assert(retrieved->power == 200);
    assert(retrieved->xp == 1000);
    assert(retrieved->realm == RealmLevel::QiRefining);
    assert(retrieved->careerLevel == 2);

    // Verify toJson(0) produces JSON with "hp": 50
    std::string json = store.toJson(0);
    assert(contains(json, "\"hp\":50"));
    assert(contains(json, "\"power\":200"));
    assert(contains(json, "\"xp\":1000"));
    assert(contains(json, "\"QiRefining\""));

    // Remove slot 0
    store.remove(0);
    assert(!store.has(0));
    assert(store.get(0) == nullptr);

    // toJson should return "{}" for absent slot
    json = store.toJson(0);
    assert(json == "{}");

    printf("  PASS: testGenericStoreBasicCrud\n");
}

// ─── Test: GenericComponentStore growTo ───────────────────────────────────────

static void testGenericStoreGrowTo() {
    ComponentSchema schema;
    schema.name = "TestComponent";
    schema.addField("value", FieldType::Int32, 0, sizeof(int32_t), "int32_t", "Value");

    struct TestComponent { int32_t value; };

    GenericComponentStore store(&schema, sizeof(TestComponent));
    assert(store.slotCount() == 0);

    // Grow to 10 slots
    store.growTo(10);
    assert(store.slotCount() == 10);

    // All slots should be absent
    for (size_t i = 0; i < 10; ++i) {
        assert(!store.has(i));
    }

    // Grow to 5 (shouldn't shrink)
    store.growTo(5);
    assert(store.slotCount() == 10);

    // Set a value at slot 9
    TestComponent tc;
    tc.value = 42;
    store.set(9, &tc);
    assert(store.has(9));

    const TestComponent* retrieved = static_cast<const TestComponent*>(store.get(9));
    assert(retrieved->value == 42);

    printf("  PASS: testGenericStoreGrowTo\n");
}

// ─── Test: GenericComponentStore multiple slots ──────────────────────────────

static void testGenericStoreMultipleSlots() {
    ComponentSchema schema;
    schema.name = "StatsComponent";
    schema.addFieldWithConstraint("hp", FieldType::Int32,
                                  offsetof(StatsComponent, hp), sizeof(int32_t),
                                  0.0f, 2147483647.0f, "Current HP");

    // Use full StatsComponent size so memory layout is correct
    GenericComponentStore store(&schema, sizeof(StatsComponent));

    StatsComponent s1;
    std::memset(&s1, 0, sizeof(s1));
    s1.hp = 100;
    store.set(0, &s1);

    StatsComponent s2;
    std::memset(&s2, 0, sizeof(s2));
    s2.hp = 200;
    store.set(5, &s2);

    assert(store.has(0));
    assert(store.has(5));
    assert(!store.has(1));
    assert(!store.has(3));

    const StatsComponent* r1 = static_cast<const StatsComponent*>(store.get(0));
    const StatsComponent* r2 = static_cast<const StatsComponent*>(store.get(5));
    assert(r1->hp == 100);
    assert(r2->hp == 200);

    // Remove slot 0, slot 5 should still be there
    store.remove(0);
    assert(!store.has(0));
    assert(store.has(5));
    assert(static_cast<const StatsComponent*>(store.get(5))->hp == 200);

    printf("  PASS: testGenericStoreMultipleSlots\n");
}

// ─── Test: DynamicComponentRegistry ──────────────────────────────────────────

static void testDynamicComponentRegistry() {
    // Register schemas in the global SchemaRegistry so pointers persist
    auto& schemaReg = SchemaRegistry::instance();

    ComponentSchema statsSchema;
    statsSchema.name = "StatsComponent";
    statsSchema.addFieldWithConstraint("hp", FieldType::Int32,
                                       offsetof(StatsComponent, hp), sizeof(int32_t),
                                       0.0f, 2147483647.0f, "Current HP");
    schemaReg.registerSchema("StatsComponent", std::move(statsSchema));

    ComponentSchema careerSchema;
    careerSchema.name = "CareerComponent";
    careerSchema.addFieldWithConstraint("totalXp", FieldType::Uint32,
                                        offsetof(CareerComponent, totalXp), sizeof(uint32_t),
                                        0.0f, 4294967295.0f, "Total career XP");
    schemaReg.registerSchema("CareerComponent", std::move(careerSchema));

    // Use pointers from SchemaRegistry (which persist)
    const ComponentSchema* statsPtr = schemaReg.getSchema("StatsComponent");
    const ComponentSchema* careerPtr = schemaReg.getSchema("CareerComponent");

    // Get registry and register
    auto& dynReg = DynamicComponentRegistry::instance();
    dynReg.registerComponent("StatsComponent", statsPtr, sizeof(StatsComponent));
    dynReg.registerComponent("CareerComponent", careerPtr, sizeof(CareerComponent));

    // Verify stores exist
    assert(dynReg.hasComponent("StatsComponent"));
    assert(dynReg.hasComponent("CareerComponent"));
    assert(!dynReg.hasComponent("Nonexistent"));

    GenericComponentStore* statsStore = dynReg.getStore("StatsComponent");
    assert(statsStore != nullptr);
    assert(statsStore->componentSize() == sizeof(StatsComponent));

    GenericComponentStore* careerStore = dynReg.getStore("CareerComponent");
    assert(careerStore != nullptr);
    assert(careerStore->componentSize() == sizeof(CareerComponent));

    // List names
    auto names = dynReg.getAllComponentNames();
    assert(names.size() >= 2);

    printf("  PASS: testDynamicComponentRegistry\n");
}

// ─── Test: allToJson ─────────────────────────────────────────────────────────

static void testAllToJson() {
    // Use a separate dynamic registry instance won't work since it's a singleton.
    // Instead, use the global one and store data.
    auto& dynReg = DynamicComponentRegistry::instance();

    // Store StatsComponent at slot 0
    GenericComponentStore* statsStore = dynReg.getStore("StatsComponent");
    assert(statsStore != nullptr);

    StatsComponent stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.hp = 50;
    stats.power = 100;
    stats.realm = RealmLevel::Mortal;
    statsStore->set(0, &stats);

    // Store CareerComponent at slot 0
    GenericComponentStore* careerStore = dynReg.getStore("CareerComponent");
    assert(careerStore != nullptr);

    CareerComponent career;
    career.totalXp = 5000;
    career.stage = CareerStage::Senior;
    career.tasksCompleted = 100;
    career.tasksSucceeded = 85;
    career.avgReviewScore = 8.5f;
    careerStore->set(0, &career);

    // allToJson should produce combined JSON
    std::string json = dynReg.allToJson(0);
    assert(contains(json, "StatsComponent"));
    assert(contains(json, "CareerComponent"));
    assert(contains(json, "\"hp\":50"));
    assert(contains(json, "\"totalXp\":5000"));

    // Clean up slot 0
    dynReg.removeAll(0);
    assert(!statsStore->has(0));
    assert(!careerStore->has(0));

    printf("  PASS: testAllToJson\n");
}

// ─── Test: initDynamicRegistryFromSchemas ────────────────────────────────────

static void testInitDynamicRegistryFromSchemas() {
    // initDynamicRegistryFromSchemas calls registerAllSchemas() internally,
    // then registers all 9 component types into DynamicComponentRegistry.
    initDynamicRegistryFromSchemas();

    auto& dynReg = DynamicComponentRegistry::instance();

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
        assert(dynReg.hasComponent(name) && "Component must be registered");
        const auto* store = dynReg.getStore(name);
        assert(store != nullptr);
        assert(store->componentSize() > 0);
        assert(store->schema() != nullptr);
    }

    auto names = dynReg.getAllComponentNames();
    assert(names.size() == 9);

    printf("  PASS: testInitDynamicRegistryFromSchemas\n");
}

// ─── Test: growAll ───────────────────────────────────────────────────────────

static void testGrowAll() {
    initDynamicRegistryFromSchemas();

    auto& dynReg = DynamicComponentRegistry::instance();

    // growAll to 100 slots
    dynReg.growAll(100);

    auto names = dynReg.getAllComponentNames();
    for (auto& name : names) {
        const auto* store = dynReg.getStore(name);
        assert(store != nullptr);
        assert(store->slotCount() >= 100);
    }

    // Verify we can store at high slots
    StatsComponent stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.hp = 999;
    dynReg.getStore("StatsComponent")->set(99, &stats);
    assert(dynReg.getStore("StatsComponent")->has(99));

    const StatsComponent* r = static_cast<const StatsComponent*>(
        dynReg.getStore("StatsComponent")->get(99));
    assert(r->hp == 999);

    // Clean up
    dynReg.getStore("StatsComponent")->remove(99);

    printf("  PASS: testGrowAll\n");
}

// ─── Test: toJson on empty store ─────────────────────────────────────────────

static void testToJsonEmptySlot() {
    ComponentSchema schema;
    schema.name = "EmptyComponent";
    schema.addField("val", FieldType::Int32, 0, sizeof(int32_t), "int32_t", "Value");

    GenericComponentStore store(&schema, sizeof(int32_t));
    store.growTo(5);

    // No component set
    assert(store.toJson(0) == "{}");
    assert(store.toJson(4) == "{}");

    printf("  PASS: testToJsonEmptySlot\n");
}

// ─── Entry point ─────────────────────────────────────────────────────────────

void runDynamicStoreTests() {
    printf("Running GenericComponentStore / DynamicComponentRegistry tests...\n");

    testGenericStoreBasicCrud();
    testGenericStoreGrowTo();
    testGenericStoreMultipleSlots();
    testDynamicComponentRegistry();
    testAllToJson();
    testInitDynamicRegistryFromSchemas();
    testGrowAll();
    testToJsonEmptySlot();

    printf("All 8 dynamic store tests PASSED.\n");
}
