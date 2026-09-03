// Tests for EventJournal — append-only event log.
#include "ecs/systems/EventJournal.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace Systems;

static void testAppendAndQuery() {
    EventJournal journal(100);
    auto id1 = journal.append(1, "xp_granted", R"({"amount":100})");
    auto id2 = journal.append(2, "decision_made", R"({"action":"execute"})");
    auto id3 = journal.append(1, "tick_completed", R"({"tickNumber":0})");

    assert(id1 == 1);
    assert(id2 == 2);
    assert(id3 == 3);
    assert(journal.size() == 3);

    auto events = journal.query(1);
    assert(events.size() == 2);
    assert(events[0].event_type == "xp_granted");
    assert(events[1].event_type == "tick_completed");

    auto events2 = journal.query(2);
    assert(events2.size() == 1);
    assert(events2[0].event_type == "decision_made");

    printf("  PASS: testAppendAndQuery\n");
}

static void testQueryAll() {
    EventJournal journal(100);
    journal.append(1, "type_a", "{}");
    journal.append(2, "type_b", "{}");
    journal.append(3, "type_c", "{}");

    auto all = journal.queryAll();
    assert(all.size() == 3);

    printf("  PASS: testQueryAll\n");
}

static void testQuerySinceId() {
    EventJournal journal(100);
    journal.append(1, "a", "{}");
    journal.append(1, "b", "{}");
    journal.append(1, "c", "{}");

    auto since2 = journal.query(1, 2);
    assert(since2.size() == 2);
    assert(since2[0].id == 2);
    assert(since2[1].id == 3);

    printf("  PASS: testQuerySinceId\n");
}

static void testRingBufferOverflow() {
    EventJournal journal(3); // very small capacity
    journal.append(1, "a", "{}"); // id=1
    journal.append(1, "b", "{}"); // id=2
    journal.append(1, "c", "{}"); // id=3
    assert(journal.size() == 3);

    journal.append(1, "d", "{}"); // id=4, overwrites id=1
    assert(journal.size() == 3);
    assert(journal.latestId() == 4);

    auto all = journal.queryAll();
    assert(all.size() == 3);
    assert(all[0].id == 2); // oldest remaining
    assert(all[2].id == 4); // newest

    printf("  PASS: testRingBufferOverflow\n");
}

static void testQueryAfterOverflow() {
    EventJournal journal(3);
    journal.append(1, "a", "{}"); // id=1
    journal.append(2, "b", "{}"); // id=2
    journal.append(1, "c", "{}"); // id=3
    journal.append(2, "d", "{}"); // id=4, overwrites id=1

    auto e1 = journal.query(1);
    assert(e1.size() == 1); // only id=3 remains for entity 1
    assert(e1[0].id == 3);

    auto e2 = journal.query(2);
    assert(e2.size() == 2); // id=2 and id=4

    printf("  PASS: testQueryAfterOverflow\n");
}

static void testClear() {
    EventJournal journal(100);
    journal.append(1, "a", "{}");
    journal.append(2, "b", "{}");
    assert(journal.size() == 2);

    journal.clear();
    assert(journal.size() == 0);
    assert(journal.empty());

    // IDs continue monotonic after clear
    auto id = journal.append(1, "c", "{}");
    assert(id == 3);

    printf("  PASS: testClear\n");
}

static void testLatestId() {
    EventJournal journal(100);
    assert(journal.latestId() == 0);

    journal.append(1, "a", "{}");
    assert(journal.latestId() == 1);

    journal.append(2, "b", "{}");
    assert(journal.latestId() == 2);

    printf("  PASS: testLatestId\n");
}

static void testTimestamp() {
    EventJournal journal(100);
    auto id = journal.append(1, "test", "{}");
    auto events = journal.queryAll();
    assert(events[0].timestamp > 0);
    assert(events[0].id == id);

    printf("  PASS: testTimestamp\n");
}

static void testEmptyQuery() {
    EventJournal journal(100);
    auto all = journal.queryAll();
    assert(all.empty());

    auto by_entity = journal.query(999);
    assert(by_entity.empty());

    printf("  PASS: testEmptyQuery\n");
}

void runEventJournalTests() {
    printf("=== test_event_journal ===\n");
    testAppendAndQuery();
    testQueryAll();
    testQuerySinceId();
    testRingBufferOverflow();
    testQueryAfterOverflow();
    testClear();
    testLatestId();
    testTimestamp();
    testEmptyQuery();
    printf("All 9 event journal tests PASSED.\n");
}
