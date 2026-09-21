// Tests for Component.h — type ID generation, Component template, ComponentRegistry.
#include "ecs/Component.h"
#include "ecs/components/StatsComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/IdentityComponent.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace ECS;

// ─── generateComponentTypeId ──────────────────────────────────────────────────

static void testGenerateTypeIds() {
    ComponentTypeId id1 = generateComponentTypeId();
    ComponentTypeId id2 = generateComponentTypeId();
    ComponentTypeId id3 = generateComponentTypeId();
    assert(id1 != id2);
    assert(id2 != id3);
    assert(id1 != id3);
    // IDs should be sequential
    assert(id2 == id1 + 1);
    assert(id3 == id2 + 1);
    printf("  PASS: testGenerateTypeIds\n");
}

// ─── IComponent virtual interface ────────────────────────────────────────────

static void testIComponentInterface() {
    // ComponentBase<T> implements IComponent
    ComponentTypeId statsId = ComponentBase<StatsComponent>::getStaticTypeId();
    ComponentTypeId persId = ComponentBase<PersonalityComponent>::getStaticTypeId();
    assert(statsId != persId);
    printf("  PASS: testIComponentInterface\n");
}

// ─── ComponentBase::getStaticTypeId ──────────────────────────────────────────

static void testGetStaticTypeIdStable() {
    // getStaticTypeId should return the same value on repeated calls
    ComponentTypeId first = ComponentBase<StatsComponent>::getStaticTypeId();
    ComponentTypeId second = ComponentBase<StatsComponent>::getStaticTypeId();
    assert(first == second);

    // Different types get different IDs
    ComponentTypeId identId = ComponentBase<IdentityComponent>::getStaticTypeId();
    assert(first != identId);
    printf("  PASS: testGetStaticTypeIdStable\n");
}

static void testGetTypeIdVirtual() {
    // Component<T>::getTypeId() should delegate to getStaticTypeId()
    StatsComponent stats;
    Component<StatsComponent> wrapper(stats);
    ComponentTypeId viaVirtual = wrapper.getTypeId();
    ComponentTypeId viaStatic = ComponentBase<StatsComponent>::getStaticTypeId();
    assert(viaVirtual == viaStatic);
    printf("  PASS: testGetTypeIdVirtual\n");
}

// ─── Component<T> constructors and accessors ─────────────────────────────────

static void testComponentDefaultConstructor() {
    Component<StatsComponent> c;
    assert(c.get().hp == 0);
    assert(c.get().maxHp == 0);
    printf("  PASS: testComponentDefaultConstructor\n");
}

static void testComponentValueConstructor() {
    StatsComponent stats;
    stats.hp = 80;
    stats.maxHp = 100;
    Component<StatsComponent> c(stats);
    assert(c.get().hp == 80);
    assert(c.get().maxHp == 100);
    printf("  PASS: testComponentValueConstructor\n");
}

static void testComponentMoveConstructor() {
    StatsComponent stats;
    stats.hp = 50;
    stats.power = 25;
    Component<StatsComponent> c(std::move(stats));
    assert(c.get().hp == 50);
    assert(c.get().power == 25);
    printf("  PASS: testComponentMoveConstructor\n");
}

static void testComponentMutableAccess() {
    Component<StatsComponent> c;
    c.get().hp = 100;
    c.get().mp = 50;
    assert(c.get().hp == 100);
    assert(c.get().mp == 50);
    printf("  PASS: testComponentMutableAccess\n");
}

static void testComponentConstAccess() {
    StatsComponent stats;
    stats.hp = 75;
    const Component<StatsComponent> c(stats);
    const StatsComponent& ref = c.get();
    assert(ref.hp == 75);
    printf("  PASS: testComponentConstAccess\n");
}

// ─── ComponentTypeInfo ───────────────────────────────────────────────────────

static void testComponentTypeInfoDefault() {
    ComponentTypeInfo info;
    assert(info.size == 0);
    assert(info.alignment == 0);
    printf("  PASS: testComponentTypeInfoDefault\n");
}

static void testComponentTypeInfoValues() {
    ComponentTypeInfo info(typeid(StatsComponent), sizeof(StatsComponent), alignof(StatsComponent));
    assert(info.size == sizeof(StatsComponent));
    assert(info.alignment == alignof(StatsComponent));
    assert(info.type == std::type_index(typeid(StatsComponent)));
    printf("  PASS: testComponentTypeInfoValues\n");
}

// ─── ComponentRegistry ───────────────────────────────────────────────────────

static void testComponentRegistryRegister() {
    ComponentRegistry& reg = ComponentRegistry::getInstance();
    size_t countBefore = reg.getComponentCount();

    reg.registerComponent<StatsComponent>();
    // Already registered by other tests, count may not change
    size_t countAfter = reg.getComponentCount();
    assert(countAfter >= countBefore);
    printf("  PASS: testComponentRegistryRegister (count=%zu)\n", countAfter);
}

static void testComponentRegistryRegisterNew() {
    struct UniqueTestComponent {
        int x;
        float y;
    };

    ComponentRegistry& reg = ComponentRegistry::getInstance();
    size_t countBefore = reg.getComponentCount();
    reg.registerComponent<UniqueTestComponent>();
    size_t countAfter = reg.getComponentCount();
    assert(countAfter == countBefore + 1);  // New type registered

    // Registering same type again should not increase count
    reg.registerComponent<UniqueTestComponent>();
    assert(reg.getComponentCount() == countAfter);
    printf("  PASS: testComponentRegistryRegisterNew\n");
}

static void testComponentRegistryGetInfo() {
    ComponentRegistry& reg = ComponentRegistry::getInstance();
    reg.registerComponent<StatsComponent>();

    ComponentTypeId id = ComponentBase<StatsComponent>::getStaticTypeId();
    ComponentTypeInfo info = reg.getComponentInfo(id);
    assert(info.size == sizeof(StatsComponent));
    assert(info.alignment == alignof(StatsComponent));
    printf("  PASS: testComponentRegistryGetInfo\n");
}

static void testComponentRegistryGetInfoUnknown() {
    ComponentRegistry& reg = ComponentRegistry::getInstance();
    ComponentTypeInfo info = reg.getComponentInfo(99999);  // Unknown ID
    assert(info.size == 0);  // Should return empty/default
    assert(info.alignment == 0);
    printf("  PASS: testComponentRegistryGetInfoUnknown\n");
}

static void testComponentRegistryCount() {
    ComponentRegistry& reg = ComponentRegistry::getInstance();
    size_t count = reg.getComponentCount();
    assert(count > 0);  // At least StatsComponent registered
    printf("  PASS: testComponentRegistryCount (%zu components)\n", count);
}

// ─── Component<T> with all component types ───────────────────────────────────

static void testComponentAllTypes() {
    // Verify Component<T> works for all component types
    Component<StatsComponent> c1;
    assert(c1.getTypeId() == ComponentBase<StatsComponent>::getStaticTypeId());

    Component<PersonalityComponent> c2;
    assert(c2.getTypeId() == ComponentBase<PersonalityComponent>::getStaticTypeId());

    Component<IdentityComponent> c3;
    assert(c3.getTypeId() == ComponentBase<IdentityComponent>::getStaticTypeId());

    printf("  PASS: testComponentAllTypes\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runComponentBaseTests() {
    printf("Running Component.h coverage tests...\n");

    testGenerateTypeIds();
    testIComponentInterface();
    testGetStaticTypeIdStable();
    testGetTypeIdVirtual();

    testComponentDefaultConstructor();
    testComponentValueConstructor();
    testComponentMoveConstructor();
    testComponentMutableAccess();
    testComponentConstAccess();

    testComponentTypeInfoDefault();
    testComponentTypeInfoValues();

    testComponentRegistryRegister();
    testComponentRegistryRegisterNew();
    testComponentRegistryGetInfo();
    testComponentRegistryGetInfoUnknown();
    testComponentRegistryCount();

    testComponentAllTypes();

    printf("All 17 Component.h coverage tests PASSED.\n");
}
