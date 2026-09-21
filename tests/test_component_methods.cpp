// Tests for component method coverage — CareerComponent, MemoryRing,
// PersonalityComponent, SchemaValidator unsigned int validation.
#include "ecs/Registry.h"
#include "ecs/Schema.h"
#include "ecs/SchemaValidator.h"
#include "ecs/ComponentSchemas.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/StatsComponent.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace ECS;

// ─── CareerComponent: XP + promotion ─────────────────────────────────────────

static void testCareerAddXp() {
    CareerComponent c;
    c.totalXp = 0;
    c.stage = CareerStage::Junior;

    c.addXp(300);
    assert(c.totalXp == 300);
    assert(c.stage == CareerStage::Junior);

    c.addXp(300);
    assert(c.totalXp == 600);
    assert(c.stage == CareerStage::Mid);
    printf("  PASS: testCareerAddXp\n");
}

static void testCareerCanPromote() {
    CareerComponent c;

    c.stage = CareerStage::Junior;
    c.totalXp = 499;
    assert(!c.canPromote());
    c.totalXp = 500;
    assert(c.canPromote());

    c.stage = CareerStage::Mid;
    c.totalXp = 1999;
    assert(!c.canPromote());
    c.totalXp = 2000;
    assert(c.canPromote());

    c.stage = CareerStage::Senior;
    c.totalXp = 4999;
    assert(!c.canPromote());
    c.totalXp = 5000;
    assert(c.canPromote());

    c.stage = CareerStage::Lead;
    c.totalXp = 9999;
    assert(!c.canPromote());
    c.totalXp = 10000;
    assert(c.canPromote());

    c.stage = CareerStage::Expert;
    c.totalXp = 99999;
    assert(!c.canPromote());

    printf("  PASS: testCareerCanPromote\n");
}

static void testCareerMultiPromotion() {
    CareerComponent c;
    c.stage = CareerStage::Junior;
    c.totalXp = 0;
    c.addXp(2500);
    assert(c.stage == CareerStage::Senior);
    assert(c.totalXp == 2500);
    printf("  PASS: testCareerMultiPromotion\n");
}

static void testCareerGetSuccessRate() {
    CareerComponent c;
    c.tasksCompleted = 0;
    c.tasksSucceeded = 0;
    assert(c.getSuccessRate() == 0.0f);

    c.tasksCompleted = 10;
    c.tasksSucceeded = 7;
    float rate = c.getSuccessRate();
    assert(rate > 0.69f && rate < 0.71f);
    printf("  PASS: testCareerGetSuccessRate\n");
}

// ─── MemoryRing: RingBuffer operations ───────────────────────────────────────

static void testRingBufferGetRecent() {
    MemoryRingComponent mem;
    for (int i = 0; i < 5; ++i) {
        InteractionSlot slot{};
        slot.timestamp = 100 + i;
        slot.otherSlot = i;
        slot.type = 1;
        slot.impactScore = static_cast<int8_t>(i);
        mem.interactions.push(slot);
    }

    InteractionSlot out[3];
    size_t n = mem.interactions.getRecent(out, 3);
    assert(n == 3);
    assert(out[0].timestamp == 104);
    assert(out[1].timestamp == 103);
    assert(out[2].timestamp == 102);
    printf("  PASS: testRingBufferGetRecent\n");
}

static void testRingBufferGetRecentMoreThanCount() {
    MemoryRingComponent mem;
    for (int i = 0; i < 2; ++i) {
        InteractionSlot slot{};
        slot.timestamp = 200 + i;
        mem.interactions.push(slot);
    }

    InteractionSlot out[10];
    size_t n = mem.interactions.getRecent(out, 10);
    assert(n == 2);
    printf("  PASS: testRingBufferGetRecentMoreThanCount\n");
}

static void testRingBufferEmpty() {
    MemoryRingComponent mem;
    assert(mem.interactions.empty());
    assert(mem.interactions.size() == 0);
    printf("  PASS: testRingBufferEmpty\n");
}

// ─── PersonalityComponent: trait accessors ───────────────────────────────────

static void testPersonalityTraits() {
    // isAggressive requires ambition>70 AND greed>50
    PersonalityComponent p(80.0f, 30.0f, 70.0f, 60.0f, 65.0f, 85.0f);

    float overall = p.getOverall();
    assert(overall > 60.0f && overall < 70.0f);

    assert(p.isAggressive());   // ambition=80>70, greed=60>50
    assert(!p.isCautious());    // caution=30<70
    assert(!p.isLoyal());       // loyalty=70, need >70
    assert(p.isSocial());       // sociability=65>60
    assert(p.isDiligent());     // diligence=85>60

    printf("  PASS: testPersonalityTraits\n");
}

static void testPersonalityLowTraits() {
    PersonalityComponent p(10.0f, 90.0f, 15.0f, 85.0f, 20.0f, 10.0f);

    assert(!p.isAggressive());  // ambition=10<70
    assert(p.isCautious());     // caution=90>70
    assert(!p.isLoyal());       // loyalty=15<70
    assert(!p.isSocial());      // sociability=20<60
    assert(!p.isDiligent());    // diligence=10<60
    printf("  PASS: testPersonalityLowTraits\n");
}

// ─── SchemaValidator: unsigned integer validation ────────────────────────────

static void testValidateUnsignedIntSchema() {
    struct UnsignedData {
        uint8_t u8;
        uint32_t u32;
        int32_t i32;
    };

    ComponentSchema schema;
    schema.name = "UnsignedTest";
    // addFieldWithConstraint(name, type, offset, size, min, max)
    schema.addFieldWithConstraint("u8", FieldType::Uint8,
                                   offsetof(UnsignedData, u8), sizeof(uint8_t), 0.0f, 100.0f);
    schema.addFieldWithConstraint("u32", FieldType::Uint32,
                                   offsetof(UnsignedData, u32), sizeof(uint32_t), 0.0f, 1000.0f);
    schema.addFieldWithConstraint("i32", FieldType::Int32,
                                   offsetof(UnsignedData, i32), sizeof(int32_t), -50.0f, 50.0f);

    UnsignedData valid{};
    valid.u8 = 50;
    valid.u32 = 500;
    valid.i32 = 0;
    ValidationResult r1 = schema.validate(&valid);
    assert(r1.valid);

    UnsignedData bad{};
    bad.u8 = 200;  // > 100
    bad.u32 = 500;
    bad.i32 = 0;
    ValidationResult r2 = schema.validate(&bad);
    assert(!r2.valid);

    UnsignedData bad2{};
    bad2.u8 = 50;
    bad2.u32 = 500;
    bad2.i32 = -100;  // < -50
    ValidationResult r3 = schema.validate(&bad2);
    assert(!r3.valid);

    printf("  PASS: testValidateUnsignedIntSchema\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runComponentMethodTests() {
    printf("Running component method coverage tests...\n");

    testCareerAddXp();
    testCareerCanPromote();
    testCareerMultiPromotion();
    testCareerGetSuccessRate();

    testRingBufferGetRecent();
    testRingBufferGetRecentMoreThanCount();
    testRingBufferEmpty();

    testPersonalityTraits();
    testPersonalityLowTraits();

    testValidateUnsignedIntSchema();

    printf("All 10 component method tests PASSED.\n");
}
