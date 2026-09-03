// Tests for AgentMailbox — per-entity message queue.
#include "ecs/systems/AgentMailbox.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace Systems;

static void testSendAndReceive() {
    AgentMailbox mailbox;
    auto id1 = mailbox.send(1, 2, R"({"text":"hello"})");
    auto id2 = mailbox.send(1, 2, R"({"text":"world"})");

    assert(id1 == 1);
    assert(id2 == 2);
    assert(mailbox.pendingCount(2) == 2);
    assert(mailbox.pendingCount(1) == 0); // no messages for entity 1

    auto msgs = mailbox.receive(2);
    assert(msgs.size() == 2);
    assert(msgs[0].payload == R"({"text":"hello"})");
    assert(msgs[0].from == 1);
    assert(msgs[0].to == 2);
    assert(msgs[0].delivered == true);

    // After receive, pending count is 0
    assert(mailbox.pendingCount(2) == 0);

    printf("  PASS: testSendAndReceive\n");
}

static void testReceiveLimit() {
    AgentMailbox mailbox;
    for (int i = 0; i < 5; i++) {
        mailbox.send(1, 2, "{}");
    }

    auto msgs = mailbox.receive(2, 3);
    assert(msgs.size() == 3);
    assert(mailbox.pendingCount(2) == 2); // 2 remaining

    printf("  PASS: testReceiveLimit\n");
}

static void testAck() {
    AgentMailbox mailbox;
    auto id = mailbox.send(1, 2, "{}");

    mailbox.receive(2);
    assert(!mailbox.getAll(2)[0].acked);

    bool ok = mailbox.ack(id);
    assert(ok);
    assert(mailbox.getAll(2)[0].acked);

    // Ack non-existent message
    assert(!mailbox.ack(999));

    printf("  PASS: testAck\n");
}

static void testMultiEntityIsolation() {
    AgentMailbox mailbox;
    mailbox.send(1, 2, "to-2");
    mailbox.send(1, 3, "to-3");
    mailbox.send(2, 3, "also-to-3");

    assert(mailbox.pendingCount(2) == 1);
    assert(mailbox.pendingCount(3) == 2);

    auto msgs2 = mailbox.receive(2);
    assert(msgs2.size() == 1);
    assert(msgs2[0].payload == "to-2");

    auto msgs3 = mailbox.receive(3);
    assert(msgs3.size() == 2);

    printf("  PASS: testMultiEntityIsolation\n");
}

static void testGetAll() {
    AgentMailbox mailbox;
    mailbox.send(1, 2, "a");
    mailbox.send(1, 2, "b");
    mailbox.receive(2);

    auto all = mailbox.getAll(2);
    assert(all.size() == 2);
    assert(all[0].delivered == true);
    assert(all[1].delivered == true);

    printf("  PASS: testGetAll\n");
}

static void testOverflow() {
    AgentMailbox mailbox(3); // max 3 per entity
    mailbox.send(1, 2, "a"); // id=1
    mailbox.send(1, 2, "b"); // id=2
    mailbox.send(1, 2, "c"); // id=3
    mailbox.send(1, 2, "d"); // id=4, drops "a"

    assert(mailbox.totalMessages() == 3);
    auto msgs = mailbox.receive(2);
    assert(msgs.size() == 3);
    assert(msgs[0].payload == "b"); // "a" was dropped

    printf("  PASS: testOverflow\n");
}

static void testTotalMessages() {
    AgentMailbox mailbox;
    assert(mailbox.totalMessages() == 0);

    mailbox.send(1, 2, "{}");
    mailbox.send(1, 3, "{}");
    assert(mailbox.totalMessages() == 2);

    printf("  PASS: testTotalMessages\n");
}

static void testClear() {
    AgentMailbox mailbox;
    mailbox.send(1, 2, "{}");
    mailbox.send(2, 3, "{}");

    mailbox.clear();
    assert(mailbox.totalMessages() == 0);
    assert(mailbox.pendingCount(2) == 0);

    // IDs continue monotonic after clear
    auto id = mailbox.send(1, 2, "{}");
    assert(id == 3);

    printf("  PASS: testClear\n");
}

static void testTimestamp() {
    AgentMailbox mailbox;
    auto id = mailbox.send(1, 2, "{}");
    auto msgs = mailbox.receive(2);
    assert(msgs[0].timestamp > 0);

    printf("  PASS: testTimestamp\n");
}

static void testEmptyReceive() {
    AgentMailbox mailbox;
    auto msgs = mailbox.receive(999);
    assert(msgs.empty());

    printf("  PASS: testEmptyReceive\n");
}

void runAgentMailboxTests() {
    printf("=== test_agent_mailbox ===\n");
    testSendAndReceive();
    testReceiveLimit();
    testAck();
    testMultiEntityIsolation();
    testGetAll();
    testOverflow();
    testTotalMessages();
    testClear();
    testTimestamp();
    testEmptyReceive();
    printf("All 10 agent mailbox tests PASSED.\n");
}
