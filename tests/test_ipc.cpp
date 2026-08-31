#include "ipc/AgentKernelBridge.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static const char* TEST_SOCKET = "/tmp/agent-kernel-test.sock";

// --- Minimal Unix socket test client ---

class TestClient {
public:
    ~TestClient() { close(); }

    bool connect(const char* path) {
        fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return false;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        // Retry connection a few times (server may not be ready yet)
        for (int attempt = 0; attempt < 50; ++attempt) {
            if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    bool send(const std::string& msg) {
        std::string line = msg + "\n";
        ssize_t n = ::send(fd_, line.data(), line.size(), MSG_NOSIGNAL);
        return n == static_cast<ssize_t>(line.size());
    }

    std::string recv() {
        std::string buffer;
        char chunk[4096];
        while (true) {
            ssize_t n = ::recv(fd_, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buffer.append(chunk, static_cast<size_t>(n));
            // Check if we have a complete line
            size_t pos = buffer.find('\n');
            if (pos != std::string::npos) {
                return buffer.substr(0, pos);
            }
        }
        return buffer;
    }

    void close() {
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    }

private:
    int fd_ = -1;
};

// --- Helper: assert JSON contains substring ---
static void assertContains(const std::string& haystack, const std::string& needle, const char* testName) {
    if (haystack.find(needle) == std::string::npos) {
        fprintf(stderr, "FAIL [%s]: expected '%s' in response: %s\n", testName, needle.c_str(), haystack.c_str());
        assert(false);
    }
}

// --- Tests ---

static void testCreateAgent(IPC::AgentKernelBridge& bridge) {
    // Clear registry first
    ECS::Registry::getInstance().clear();

    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"createAgent","params":{"id":"agent-001","name":"Alice","department":"Engineering","companyRole":"Developer","teamId":"team-alpha","role":"Worker"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "createAgent:ok");
    assertContains(resp, "agent-001", "createAgent:id");
    assertContains(resp, "Alice", "createAgent:name");
    assertContains(resp, "Engineering", "createAgent:dept");

    printf("  PASS: testCreateAgent\n");
}

static void testGetAgent(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    // The agent created in testCreateAgent should be entityId 0
    std::string req = R"({"method":"getAgent","params":{"entityId":0}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "getAgent:ok");
    assertContains(resp, "agent-001", "getAgent:id");
    assertContains(resp, "Alice", "getAgent:name");
    assertContains(resp, "\"entityId\":0", "getAgent:entityId");

    printf("  PASS: testGetAgent\n");
}

static void testListAgents(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"listAgents"})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "listAgents:ok");
    assertContains(resp, "agent-001", "listAgents:hasAgent");

    printf("  PASS: testListAgents\n");
}

static void testUpdateAgent(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"updateAgent","params":{"entityId":0,"name":"Alice Updated","role":"Lead"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "updateAgent:ok");
    assertContains(resp, "Alice Updated", "updateAgent:name");
    assertContains(resp, "Lead", "updateAgent:role");

    printf("  PASS: testUpdateAgent\n");
}

static void testDeleteAgent(IPC::AgentKernelBridge& /*bridge*/) {
    // First create a second agent to delete
    {
        TestClient client;
        assert(client.connect(TEST_SOCKET));
        std::string req = R"({"method":"createAgent","params":{"id":"agent-002","name":"Bob","department":"Design","companyRole":"Designer","teamId":"team-beta","role":"Specialist"}})";
        assert(client.send(req));
        std::string resp = client.recv();
        assertContains(resp, "\"ok\":true", "deleteAgent:create");
    }

    // Get the entityId of Bob (should be 1, since we have Alice at 0)
    ECS::EntityId bobId = 1;

    // Delete Bob
    {
        TestClient client;
        assert(client.connect(TEST_SOCKET));
        std::string req = "{\"method\":\"deleteAgent\",\"params\":{\"entityId\":" + std::to_string(bobId) + "}}";
        assert(client.send(req));
        std::string resp = client.recv();
        assertContains(resp, "\"ok\":true", "deleteAgent:ok");
        assertContains(resp, "true", "deleteAgent:deleted");
    }

    // Verify Bob is gone
    {
        TestClient client;
        assert(client.connect(TEST_SOCKET));
        std::string req = "{\"method\":\"getAgent\",\"params\":{\"entityId\":" + std::to_string(bobId) + "}}";
        assert(client.send(req));
        std::string resp = client.recv();
        assertContains(resp, "\"ok\":false", "deleteAgent:verifGone");
    }

    printf("  PASS: testDeleteAgent\n");
}

static void testAddSkillXp(IPC::AgentKernelBridge& /*bridge*/) {
    // Alice (entityId 0) has a SkillTreeComponent, but no skills yet.
    // Add a skill first via direct registry access, then test addSkillXp via IPC.
    {
        auto& reg = ECS::Registry::getInstance();
        auto* tree = reg.getComponent<SkillTreeComponent>(0);
        assert(tree != nullptr);
        tree->addSkill("cpp", SkillCategory::Engineering, SkillLevel::Beginner);
    }

    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"addSkillXp","params":{"entityId":0,"skillId":"cpp","xp":500}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "addSkillXp:ok");
    assertContains(resp, "500", "addSkillXp:xp");

    printf("  PASS: testAddSkillXp\n");
}

static void testGetSkills(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"getSkills","params":{"entityId":0}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "getSkills:ok");
    assertContains(resp, "cpp", "getSkills:hasSkill");

    printf("  PASS: testGetSkills\n");
}

static void testSyncState(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"syncState"})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "syncState:ok");
    assertContains(resp, "\"count\":", "syncState:count");
    assertContains(resp, "agent-001", "syncState:hasAgent");

    printf("  PASS: testSyncState\n");
}

static void testUnknownMethod(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"method":"nonExistent"})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":false", "unknownMethod:ok");
    assertContains(resp, "unknown method", "unknownMethod:error");

    printf("  PASS: testUnknownMethod\n");
}

static void testMissingMethod(IPC::AgentKernelBridge& /*bridge*/) {
    TestClient client;
    assert(client.connect(TEST_SOCKET));

    std::string req = R"({"params":{"entityId":0}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":false", "missingMethod:ok");
    assertContains(resp, "missing method", "missingMethod:error");

    printf("  PASS: testMissingMethod\n");
}

// --- Entry point ---

void runIpcTests() {
    printf("Running IPC bridge tests...\n");

    // Clear registry
    ECS::Registry::getInstance().clear();

    // Start bridge on test socket
    unlink(TEST_SOCKET);
    IPC::AgentKernelBridge bridge(TEST_SOCKET);
    assert(bridge.start());
    assert(bridge.isRunning());

    // Give server thread a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run tests in order (they build on each other)
    testCreateAgent(bridge);
    testGetAgent(bridge);
    testListAgents(bridge);
    testUpdateAgent(bridge);
    testDeleteAgent(bridge);
    testAddSkillXp(bridge);
    testGetSkills(bridge);
    testSyncState(bridge);
    testUnknownMethod(bridge);
    testMissingMethod(bridge);

    // Cleanup
    bridge.stop();
    assert(!bridge.isRunning());

    printf("All 10 IPC tests PASSED.\n");
}
