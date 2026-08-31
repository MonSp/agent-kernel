// test_ipc_archetype.cpp — Tests for archetype IPC endpoints
//
// Tests: listArchetypes, createFromArchetype via IPC

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

static const char* ARCHETYPE_TEST_SOCKET = "/tmp/agent-kernel-archetype-test.sock";

// --- Minimal Unix socket test client ---

class ArchetypeTestClient {
public:
    ~ArchetypeTestClient() { close(); }

    bool connect(const char* path) {
        fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return false;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

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
        char chunk[8192];
        while (true) {
            ssize_t n = ::recv(fd_, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buffer.append(chunk, static_cast<size_t>(n));
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

// --- Helpers ---

static void assertContains(const std::string& haystack, const std::string& needle, const char* testName) {
    if (haystack.find(needle) == std::string::npos) {
        fprintf(stderr, "FAIL [%s]: expected '%s' in response:\n%s\n", testName, needle.c_str(), haystack.c_str());
        assert(false);
    }
}

// --- Test: listArchetypes returns all 6 builtin archetypes ---

static void testListArchetypes() {
    ArchetypeTestClient client;
    assert(client.connect(ARCHETYPE_TEST_SOCKET));

    std::string req = R"({"method":"listArchetypes"})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "listArchetypes:ok");

    // All 6 builtin archetypes should be present
    assertContains(resp, "\"Engineer\"", "listArchetypes:Engineer");
    assertContains(resp, "\"Designer\"", "listArchetypes:Designer");
    assertContains(resp, "\"Manager\"", "listArchetypes:Manager");
    assertContains(resp, "\"Warrior\"", "listArchetypes:Warrior");
    assertContains(resp, "\"Alchemist\"", "listArchetypes:Alchemist");
    assertContains(resp, "\"Elder\"", "listArchetypes:Elder");

    // Descriptions should be present
    assertContains(resp, "\"description\":", "listArchetypes:hasDescription");

    // Component lists should be present
    assertContains(resp, "\"components\":", "listArchetypes:hasComponents");
    assertContains(resp, "IdentityComponent", "listArchetypes:IdentityComponent");
    assertContains(resp, "StatsComponent", "listArchetypes:StatsComponent");

    printf("  PASS: testListArchetypes\n");
}

// --- Test: createFromArchetype with "Warrior" creates entity with Stats (hp=100) ---

static void testCreateFromArchetypeWarrior() {
    ArchetypeTestClient client;
    assert(client.connect(ARCHETYPE_TEST_SOCKET));

    std::string req = R"({"method":"createFromArchetype","params":{"archetype":"Warrior"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "createFromWarrior:ok");
    assertContains(resp, "\"stats\":", "createFromWarrior:hasStats");
    assertContains(resp, "\"hp\":100", "createFromWarrior:hp100");
    assertContains(resp, "\"maxHp\":100", "createFromWarrior:maxHp100");
    assertContains(resp, "\"mp\":50", "createFromWarrior:mp50");
    assertContains(resp, "\"maxMp\":50", "createFromWarrior:maxMp50");

    // Should also have identity, personality, lifecycle, skillTree, career
    assertContains(resp, "\"identity\":", "createFromWarrior:hasIdentity");
    assertContains(resp, "\"personality\":", "createFromWarrior:hasPersonality");
    assertContains(resp, "\"lifecycle\":", "createFromWarrior:hasLifecycle");
    assertContains(resp, "\"skillTree\":", "createFromWarrior:hasSkillTree");
    assertContains(resp, "\"career\":", "createFromWarrior:hasCareer");

    printf("  PASS: testCreateFromArchetypeWarrior\n");
}

// --- Test: createFromArchetype with identity overrides ---

static void testCreateFromArchetypeWithOverrides() {
    ArchetypeTestClient client;
    assert(client.connect(ARCHETYPE_TEST_SOCKET));

    std::string req = R"({"method":"createFromArchetype","params":{"archetype":"Engineer","name":"XiaoMing","department":"engineering"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "createFromOverride:ok");
    assertContains(resp, "\"identity\":", "createFromOverride:hasIdentity");
    assertContains(resp, "\"name\":\"XiaoMing\"", "createFromOverride:name");
    assertContains(resp, "\"department\":\"engineering\"", "createFromOverride:department");

    // Engineer archetype should have its component set
    assertContains(resp, "\"skillTree\":", "createFromOverride:hasSkillTree");
    assertContains(resp, "\"career\":", "createFromOverride:hasCareer");
    assertContains(resp, "\"evolution\":", "createFromOverride:hasEvolution");

    // Should NOT have Stats (Engineer doesn't include Stats)
    // Check absence by verifying the response is valid but stats is not present
    assert(resp.find("\"stats\":") == std::string::npos);

    printf("  PASS: testCreateFromArchetypeWithOverrides\n");
}

// --- Test: createFromArchetype with invalid archetype name returns error ---

static void testCreateFromArchetypeInvalid() {
    ArchetypeTestClient client;
    assert(client.connect(ARCHETYPE_TEST_SOCKET));

    std::string req = R"({"method":"createFromArchetype","params":{"archetype":"NonExistent"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":false", "createFromInvalid:ok");
    assertContains(resp, "unknown archetype", "createFromInvalid:errorMsg");

    printf("  PASS: testCreateFromArchetypeInvalid\n");
}

// --- Entry point ---

void runArchetypeIpcTests() {
    printf("Running archetype IPC tests...\n");

    // Clear registry
    ECS::Registry::getInstance().clear();

    // Ensure schemas are registered
    registerAllSchemas();
    registerBuiltinArchetypes();

    // Start bridge on test socket
    unlink(ARCHETYPE_TEST_SOCKET);
    IPC::AgentKernelBridge bridge(ARCHETYPE_TEST_SOCKET);
    assert(bridge.start());
    assert(bridge.isRunning());

    // Give server thread a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run tests
    testListArchetypes();
    testCreateFromArchetypeWarrior();
    testCreateFromArchetypeWithOverrides();
    testCreateFromArchetypeInvalid();

    // Cleanup
    bridge.stop();
    assert(!bridge.isRunning());

    printf("All 4 archetype IPC tests PASSED.\n");
}
