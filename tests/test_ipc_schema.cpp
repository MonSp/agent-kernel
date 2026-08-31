// test_ipc_schema.cpp — Tests for schema IPC endpoints and all-9-component exposure
//
// Tests: getSchemas, getSchema, describeEntity, and getAgent with all components

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

static const char* SCHEMA_TEST_SOCKET = "/tmp/agent-kernel-schema-test.sock";

// --- Minimal Unix socket test client (duplicated from test_ipc.cpp for independence) ---

class SchemaTestClient {
public:
    ~SchemaTestClient() { close(); }

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

// --- Helper ---

static void assertContains(const std::string& haystack, const std::string& needle, const char* testName) {
    if (haystack.find(needle) == std::string::npos) {
        fprintf(stderr, "FAIL [%s]: expected '%s' in response:\n%s\n", testName, needle.c_str(), haystack.c_str());
        assert(false);
    }
}

static void assertNotContains(const std::string& haystack, const std::string& needle, const char* testName) {
    if (haystack.find(needle) != std::string::npos) {
        fprintf(stderr, "FAIL [%s]: did NOT expect '%s' in response:\n%s\n", testName, needle.c_str(), haystack.c_str());
        assert(false);
    }
}

// --- Test: getSchemas returns all 9 schema names ---

static void testGetSchemas() {
    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    std::string req = R"({"method":"getSchemas"})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "getSchemas:ok");
    assertContains(resp, "IdentityComponent", "getSchemas:IdentityComponent");
    assertContains(resp, "StatsComponent", "getSchemas:StatsComponent");
    assertContains(resp, "PersonalityComponent", "getSchemas:PersonalityComponent");
    assertContains(resp, "LifecycleComponent", "getSchemas:LifecycleComponent");
    assertContains(resp, "SocialComponent", "getSchemas:SocialComponent");
    assertContains(resp, "MemoryRingComponent", "getSchemas:MemoryRingComponent");
    assertContains(resp, "SkillTreeComponent", "getSchemas:SkillTreeComponent");
    assertContains(resp, "CareerComponent", "getSchemas:CareerComponent");
    assertContains(resp, "EvolutionComponent", "getSchemas:EvolutionComponent");

    printf("  PASS: testGetSchemas\n");
}

// --- Test: getSchema returns valid JSON Schema for StatsComponent ---

static void testGetSchemaStatsComponent() {
    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    std::string req = R"({"method":"getSchema","params":{"componentName":"StatsComponent"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "getSchema:ok");
    assertContains(resp, "StatsComponent", "getSchema:name");
    assertContains(resp, "\"type\":", "getSchema:hasType");
    assertContains(resp, "\"properties\":", "getSchema:hasProperties");
    // Verify field definitions
    assertContains(resp, "\"hp\"", "getSchema:hasHp");
    assertContains(resp, "\"maxHp\"", "getSchema:hasMaxHp");
    assertContains(resp, "\"power\"", "getSchema:hasPower");
    assertContains(resp, "\"realm\"", "getSchema:hasRealm");

    printf("  PASS: testGetSchemaStatsComponent\n");
}

// --- Test: getSchema returns valid schema for PersonalityComponent ---

static void testGetSchemaPersonalityComponent() {
    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    std::string req = R"({"method":"getSchema","params":{"componentName":"PersonalityComponent"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "getSchema:ok");
    assertContains(resp, "PersonalityComponent", "getSchema:name");
    assertContains(resp, "\"ambition\"", "getSchema:hasAmbition");
    assertContains(resp, "\"caution\"", "getSchema:hasCaution");
    assertContains(resp, "\"loyalty\"", "getSchema:hasLoyalty");

    printf("  PASS: testGetSchemaPersonalityComponent\n");
}

// --- Test: getSchema returns error for unknown component ---

static void testGetSchemaUnknown() {
    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    std::string req = R"({"method":"getSchema","params":{"componentName":"NonExistentComponent"}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":false", "getSchemaUnknown:ok");
    assertContains(resp, "schema not found", "getSchemaUnknown:error");

    printf("  PASS: testGetSchemaUnknown\n");
}

// --- Test: describeEntity returns all attached components ---

static void testDescribeEntity() {
    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    // Create an agent (entityId should be 0 since registry was cleared)
    {
        SchemaTestClient createClient;
        assert(createClient.connect(SCHEMA_TEST_SOCKET));
        std::string createReq = R"({"method":"createAgent","params":{"id":"schema-agent-001","name":"Schema Tester","department":"QA","companyRole":"Tester","teamId":"team-schema","role":"Specialist"}})";
        assert(createClient.send(createReq));
        std::string createResp = createClient.recv();
        assertContains(createResp, "\"ok\":true", "describeEntity:create");
    }

    // Describe the entity
    std::string req = R"({"method":"describeEntity","params":{"entityId":0}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "describeEntity:ok");
    assertContains(resp, "\"entityId\":0", "describeEntity:entityId");
    assertContains(resp, "\"components\":", "describeEntity:components");

    // All 9 components should be present (createAgent now attaches all 9)
    assertContains(resp, "IdentityComponent", "describeEntity:IdentityComponent");
    assertContains(resp, "StatsComponent", "describeEntity:StatsComponent");
    assertContains(resp, "PersonalityComponent", "describeEntity:PersonalityComponent");
    assertContains(resp, "LifecycleComponent", "describeEntity:LifecycleComponent");
    assertContains(resp, "SocialComponent", "describeEntity:SocialComponent");
    assertContains(resp, "MemoryRingComponent", "describeEntity:MemoryRingComponent");
    assertContains(resp, "SkillTreeComponent", "describeEntity:SkillTreeComponent");
    assertContains(resp, "CareerComponent", "describeEntity:CareerComponent");
    assertContains(resp, "EvolutionComponent", "describeEntity:EvolutionComponent");

    printf("  PASS: testDescribeEntity\n");
}

// --- Test: describeEntity with set values returns correct data ---

static void testDescribeEntityWithValues() {
    // Set some values on entity 0 via direct registry access
    auto& reg = ECS::Registry::getInstance();

    auto* stats = reg.getComponent<StatsComponent>(0);
    assert(stats != nullptr);
    stats->power = 42;
    stats->hp = 100;
    stats->maxHp = 200;
    stats->mp = 50;
    stats->maxMp = 80;
    stats->xp = 1234;

    auto* personality = reg.getComponent<PersonalityComponent>(0);
    assert(personality != nullptr);
    personality->ambition = 80.0f;
    personality->caution = 30.0f;
    personality->loyalty = 90.0f;

    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    std::string req = R"({"method":"describeEntity","params":{"entityId":0}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "describeEntityValues:ok");

    // Stats values (via schema-driven instanceToJson — uses field names from schema)
    assertContains(resp, "42", "describeEntityValues:power");
    assertContains(resp, "100", "describeEntityValues:hp");
    assertContains(resp, "200", "describeEntityValues:maxHp");
    assertContains(resp, "1234", "describeEntityValues:xp");

    // Personality values (via schema-driven instanceToJson)
    assertContains(resp, "80", "describeEntityValues:ambition");
    assertContains(resp, "90", "describeEntityValues:loyalty");

    printf("  PASS: testDescribeEntityWithValues\n");
}

// --- Test: getAgent now includes Personality and Stats (not just Identity/SkillTree/Career) ---

static void testGetAgentIncludesAllComponents() {
    SchemaTestClient client;
    assert(client.connect(SCHEMA_TEST_SOCKET));

    std::string req = R"({"method":"getAgent","params":{"entityId":0}})";
    assert(client.send(req));
    std::string resp = client.recv();

    assertContains(resp, "\"ok\":true", "getAgentAll:ok");

    // Should now include all component keys
    assertContains(resp, "\"identity\":", "getAgentAll:hasIdentity");
    assertContains(resp, "\"stats\":", "getAgentAll:hasStats");
    assertContains(resp, "\"personality\":", "getAgentAll:hasPersonality");
    assertContains(resp, "\"lifecycle\":", "getAgentAll:hasLifecycle");
    assertContains(resp, "\"social\":", "getAgentAll:hasSocial");
    assertContains(resp, "\"memory\":", "getAgentAll:hasMemory");
    assertContains(resp, "\"skillTree\":", "getAgentAll:hasSkillTree");
    assertContains(resp, "\"career\":", "getAgentAll:hasCareer");
    assertContains(resp, "\"evolution\":", "getAgentAll:hasEvolution");

    // Stats data should reflect the values we set
    assertContains(resp, "\"power\":42", "getAgentAll:statsPower");
    assertContains(resp, "\"hp\":100", "getAgentAll:statsHp");

    printf("  PASS: testGetAgentIncludesAllComponents\n");
}

// --- Entry point ---

void runSchemaIpcTests() {
    printf("Running schema IPC tests...\n");

    // Clear registry
    ECS::Registry::getInstance().clear();

    // Ensure schemas are registered
    registerAllSchemas();

    // Start bridge on test socket
    unlink(SCHEMA_TEST_SOCKET);
    IPC::AgentKernelBridge bridge(SCHEMA_TEST_SOCKET);
    assert(bridge.start());
    assert(bridge.isRunning());

    // Give server thread a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run tests
    testGetSchemas();
    testGetSchemaStatsComponent();
    testGetSchemaPersonalityComponent();
    testGetSchemaUnknown();
    testDescribeEntity();
    testDescribeEntityWithValues();
    testGetAgentIncludesAllComponents();

    // Cleanup
    bridge.stop();
    assert(!bridge.isRunning());

    printf("All 7 schema IPC tests PASSED.\n");
}
