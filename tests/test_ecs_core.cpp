#include "agent_kernel.h"
#include <cassert>
#include <cstdio>
#include <set>

using namespace ECS;

static void testCreateEntity() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    assert(e.isValid());
    assert(reg.getEntityCount() == 1);
    assert(reg.isEntityValid(e.getId()));

    printf("  PASS: testCreateEntity\n");
}

static void testMultipleEntitiesUniqueIds() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    constexpr int N = 100;
    std::set<EntityId> ids;
    for (int i = 0; i < N; ++i) {
        Entity e = reg.createEntity();
        assert(e.isValid());
        bool inserted = ids.insert(e.getId()).second;
        assert(inserted && "Entity IDs must be unique");
    }
    assert(reg.getEntityCount() == N);

    printf("  PASS: testMultipleEntitiesUniqueIds\n");
}

static void testDestroyEntity() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e1 = reg.createEntity();
    Entity e2 = reg.createEntity();
    assert(reg.getEntityCount() == 2);

    reg.destroyEntity(e1.getId());
    assert(reg.getEntityCount() == 1);
    assert(!reg.isEntityValid(e1.getId()));
    assert(reg.isEntityValid(e2.getId()));

    printf("  PASS: testDestroyEntity\n");
}

static void testSlotReuse() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e1 = reg.createEntity();
    EntityId id1 = e1.getId();
    reg.destroyEntity(id1);

    Entity e2 = reg.createEntity();
    // The slot should be reused; the new entity gets a recycled ID from freeIds_
    // so e2.getId() == id1
    assert(e2.getId() == id1);
    assert(reg.isEntityValid(e2.getId()));
    assert(reg.getEntityCount() == 1);

    printf("  PASS: testSlotReuse\n");
}

static void testClear() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    for (int i = 0; i < 50; ++i) {
        reg.createEntity();
    }
    assert(reg.getEntityCount() == 50);

    reg.clear();
    assert(reg.getEntityCount() == 0);
    assert(reg.getAllEntities().empty());

    // After clear, new IDs start from 0 again
    Entity e = reg.createEntity();
    assert(e.getId() == 0);

    printf("  PASS: testClear\n");
}

extern void runComponentTests();
extern void runNewComponentTests();
extern void runIpcTests();

int main() {
    printf("Running agent-kernel ECS core tests...\n");

    testCreateEntity();
    testMultipleEntitiesUniqueIds();
    testDestroyEntity();
    testSlotReuse();
    testClear();

    printf("All 5 core tests PASSED.\n\n");

    runComponentTests();

    printf("\n");
    runNewComponentTests();

    printf("\n");
    runIpcTests();

    printf("\nAll tests PASSED.\n");
    return 0;
}
