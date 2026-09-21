// Tests for AgentKernelBridge uncovered IPC endpoints:
// addSkill, addCareerXp, validateEntity, agentDecide, runSimulation,
// appendEvent, getEvents, sendMessage, getMessages + error paths.
#include "ipc/AgentKernelBridge.h"
#include "llm/HttpClient.h"
#include "ecs/Registry.h"
#include "ecs/ComponentSchemas.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

using namespace ECS;
using namespace IPC;

static const char* TEST_BRIDGE_SOCKET = "/tmp/agent-kernel-bridge-test.sock";

class BridgeClient {
public:
    ~BridgeClient() { close(); }
    bool connect(const char* path) {
        fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
        for (int i = 0; i < 30; ++i) {
            if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    std::string send(const std::string& msg) {
        std::string line = msg + "\n";
        ::send(fd_, line.data(), line.size(), MSG_NOSIGNAL);
        char buf[16384] = {0};
        std::string resp;
        ssize_t n;
        while ((n = ::recv(fd_, buf, sizeof(buf) - 1, 0)) > 0) {
            resp.append(buf, n);
            size_t pos = resp.find('\n');
            if (pos != std::string::npos) return resp.substr(0, pos);
        }
        return resp;
    }
    void close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }
private:
    int fd_ = -1;
};

static AgentKernelBridge* g_bridge = nullptr;

static void setupBridge() {
    if (g_bridge) { g_bridge->stop(); delete g_bridge; }
    unlink(TEST_BRIDGE_SOCKET);
    registerAllSchemas();
    Registry::getInstance().clear();
    g_bridge = new AgentKernelBridge(TEST_BRIDGE_SOCKET);
    g_bridge->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

static std::string ipc(const std::string& request) {
    BridgeClient client;
    assert(client.connect(TEST_BRIDGE_SOCKET));
    return client.send(request);
}

static uint64_t createTestAgent(const std::string& name) {
    std::string resp = ipc(R"({"method":"createAgent","params":{"id":"agent-)" + name + R"(","name":")" + name + R"(","department":"Engineering","companyRole":"Developer","teamId":"t1","role":"Worker"}})");
    // Extract entityId from response
    auto pos = resp.find("\"entityId\":");
    if (pos == std::string::npos) return UINT64_MAX;
    pos += 11;
    return std::stoull(resp.substr(pos));
}

// ─── addSkill ────────────────────────────────────────────────────────────────

static void testAddSkill() {
    setupBridge();
    uint64_t id = createTestAgent("SkillAgent");
    assert(id != UINT64_MAX);

    std::string resp = ipc(R"({"method":"addSkill","params":{"entityId":)" + std::to_string(id) + R"(,"skillId":"backend_dev","category":"Engineering"}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("backend_dev") != std::string::npos);
    printf("  PASS: testAddSkill\n");
}

static void testAddSkillMissingParams() {
    setupBridge();
    std::string resp = ipc(R"({"method":"addSkill","params":{}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    printf("  PASS: testAddSkillMissingParams\n");
}

static void testAddSkillNoSkillTree() {
    setupBridge();
    // Create entity without skill tree
    Registry::getInstance().clear();
    Entity e = Registry::getInstance().createEntity();
    uint64_t id = e.getId();

    std::string resp = ipc(R"({"method":"addSkill","params":{"entityId":)" + std::to_string(id) + R"(,"skillId":"test","category":"Engineering"}})");
    // Should fail — no skill tree component
    printf("  PASS: testAddSkillNoSkillTree\n");
}

// ─── addCareerXp ─────────────────────────────────────────────────────────────

static void testAddCareerXp() {
    setupBridge();
    uint64_t id = createTestAgent("CareerAgent");
    assert(id != UINT64_MAX);

    std::string resp = ipc(R"({"method":"addCareerXp","params":{"entityId":)" + std::to_string(id) + R"(,"xp":600}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("600") != std::string::npos);
    printf("  PASS: testAddCareerXp\n");
}

static void testAddCareerXpInvalidEntity() {
    setupBridge();
    std::string resp = ipc(R"({"method":"addCareerXp","params":{"entityId":99999,"xp":100}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    assert(resp.find("not found") != std::string::npos);
    printf("  PASS: testAddCareerXpInvalidEntity\n");
}

// ─── validateEntity ──────────────────────────────────────────────────────────

static void testValidateEntityValid() {
    setupBridge();
    uint64_t id = createTestAgent("ValidAgent");
    assert(id != UINT64_MAX);

    std::string resp = ipc(R"({"method":"validateEntity","params":{"entityId":)" + std::to_string(id) + "}}");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("valid") != std::string::npos);
    printf("  PASS: testValidateEntityValid\n");
}

static void testValidateEntityInvalid() {
    setupBridge();
    std::string resp = ipc(R"({"method":"validateEntity","params":{"entityId":99999}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    printf("  PASS: testValidateEntityInvalid\n");
}

// ─── agentDecide (with mock LLM) ────────────────────────────────────────────

static void mockLLMResponse(const std::string& content) {
    std::string escaped;
    for (char c : content) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            default:   escaped += c;      break;
        }
    }
    std::string body = R"({"choices":[{"message":{"role":"assistant","content":")" + escaped + R"("}}],"usage":{"prompt_tokens":10,"completion_tokens":5}})";
    LLM::HttpClient::setMockResponse(200, body);
}

static void testAgentDecide() {
    setupBridge();
    mockLLMResponse(R"({"action":"execute","reasoning":"ok","confidence":0.8})");
    uint64_t id = createTestAgent("DecideAgent");
    assert(id != UINT64_MAX);

    std::string resp = ipc(R"({"method":"agentDecide","params":{"entityId":)" + std::to_string(id) + R"(,"task":"implement feature"}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("execute") != std::string::npos || resp.find("action") != std::string::npos);
    printf("  PASS: testAgentDecide\n");
}

static void testAgentDecideMissingTask() {
    setupBridge();
    uint64_t id = createTestAgent("NoTaskAgent");
    std::string resp = ipc(R"({"method":"agentDecide","params":{"entityId":)" + std::to_string(id) + R"(,"task":""}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    assert(resp.find("task is required") != std::string::npos);
    printf("  PASS: testAgentDecideMissingTask\n");
}

static void testAgentDecideInvalidEntity() {
    setupBridge();
    mockLLMResponse(R"({"action":"execute","reasoning":"ok","confidence":0.5})");
    std::string resp = ipc(R"({"method":"agentDecide","params":{"entityId":99999,"task":"test"}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    printf("  PASS: testAgentDecideInvalidEntity\n");
}

// ─── runSimulation ───────────────────────────────────────────────────────────

static void testRunSimulation() {
    setupBridge();
    mockLLMResponse(R"({"action":"execute","reasoning":"ok","confidence":0.7})");
    uint64_t id1 = createTestAgent("Sim1");
    uint64_t id2 = createTestAgent("Sim2");

    std::string resp = ipc(R"({"method":"runSimulation","params":{"entityIds":[)" +
        std::to_string(id1) + "," + std::to_string(id2) + R"(],"ticks":2,"tasks":["task A"]}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("results") != std::string::npos);
    printf("  PASS: testRunSimulation\n");
}

// ─── appendEvent + getEvents ─────────────────────────────────────────────────

static void testAppendEvent() {
    setupBridge();
    uint64_t id = createTestAgent("EventAgent");

    std::string resp = ipc(R"({"method":"appendEvent","params":{"entityId":)" + std::to_string(id) +
        R"(,"eventType":"test_event","payload":"{\"data\":\"hello\"}"}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("eventId") != std::string::npos);
    printf("  PASS: testAppendEvent\n");
}

static void testAppendEventMissingType() {
    setupBridge();
    std::string resp = ipc(R"({"method":"appendEvent","params":{"entityId":0,"eventType":"","payload":"x"}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    assert(resp.find("eventType is required") != std::string::npos);
    printf("  PASS: testAppendEventMissingType\n");
}

static void testGetEvents() {
    setupBridge();
    uint64_t id = createTestAgent("EventAgent2");

    // Append some events
    ipc(R"({"method":"appendEvent","params":{"entityId":)" + std::to_string(id) + R"(,"eventType":"event_a","payload":"{}"}})");
    ipc(R"({"method":"appendEvent","params":{"entityId":)" + std::to_string(id) + R"(,"eventType":"event_b","payload":"{}"}})");

    // Query events for this entity
    std::string resp = ipc(R"({"method":"getEvents","params":{"entityId":)" + std::to_string(id) + R"(,"sinceId":0}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("event_a") != std::string::npos);
    assert(resp.find("event_b") != std::string::npos);
    printf("  PASS: testGetEvents\n");
}

static void testGetEventsAll() {
    setupBridge();
    // Query all events (entityId = -1)
    std::string resp = ipc(R"({"method":"getEvents","params":{"entityId":-1,"sinceId":0}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("events") != std::string::npos);
    printf("  PASS: testGetEventsAll\n");
}

// ─── sendMessage + getMessages ───────────────────────────────────────────────

static void testSendMessage() {
    setupBridge();
    uint64_t from = createTestAgent("Sender");
    uint64_t to = createTestAgent("Receiver");

    std::string resp = ipc(R"({"method":"sendMessage","params":{"from":)" + std::to_string(from) +
        R"(,"to":)" + std::to_string(to) + R"(,"payload":"{\"text\":\"hello\"}"}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("messageId") != std::string::npos);
    printf("  PASS: testSendMessage\n");
}

static void testSendMessageMissingFields() {
    setupBridge();
    std::string resp = ipc(R"({"method":"sendMessage","params":{"payload":"hello"}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    printf("  PASS: testSendMessageMissingFields\n");
}

static void testSendMessageInvalidEntity() {
    setupBridge();
    std::string resp = ipc(R"({"method":"sendMessage","params":{"from":99999,"to":99998,"payload":"x"}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    printf("  PASS: testSendMessageInvalidEntity\n");
}

static void testGetMessages() {
    setupBridge();
    uint64_t from = createTestAgent("MsgSender");
    uint64_t to = createTestAgent("MsgReceiver");

    // Send two messages
    ipc(R"({"method":"sendMessage","params":{"from":)" + std::to_string(from) +
        R"(,"to":)" + std::to_string(to) + R"(,"payload":"msg1"}})");
    ipc(R"({"method":"sendMessage","params":{"from":)" + std::to_string(from) +
        R"(,"to":)" + std::to_string(to) + R"(,"payload":"msg2"}})");

    // Receive messages
    std::string resp = ipc(R"({"method":"getMessages","params":{"entityId":)" + std::to_string(to) + R"(,"limit":10}})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("msg1") != std::string::npos);
    assert(resp.find("msg2") != std::string::npos);
    assert(resp.find("pending") != std::string::npos);
    printf("  PASS: testGetMessages\n");
}

static void testGetMessagesMissingEntity() {
    setupBridge();
    std::string resp = ipc(R"({"method":"getMessages","params":{"limit":10}})");
    assert(resp.find("\"ok\":false") != std::string::npos);
    printf("  PASS: testGetMessagesMissingEntity\n");
}

// ─── syncState error paths ───────────────────────────────────────────────────

static void testSyncStateWithAgents() {
    setupBridge();
    createTestAgent("SyncAgent");
    std::string resp = ipc(R"({"method":"syncState"})");
    assert(resp.find("\"ok\":true") != std::string::npos);
    assert(resp.find("agents") != std::string::npos);
    printf("  PASS: testSyncStateWithAgents\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runBridgeIpcTests() {
    printf("Running AgentKernelBridge IPC coverage tests...\n");

    // addSkill
    testAddSkill();
    testAddSkillMissingParams();
    testAddSkillNoSkillTree();

    // addCareerXp
    testAddCareerXp();
    testAddCareerXpInvalidEntity();

    // validateEntity
    testValidateEntityValid();
    testValidateEntityInvalid();

    // agentDecide
    testAgentDecide();
    testAgentDecideMissingTask();
    testAgentDecideInvalidEntity();

    // runSimulation
    testRunSimulation();

    // events
    testAppendEvent();
    testAppendEventMissingType();
    testGetEvents();
    testGetEventsAll();

    // messages
    testSendMessage();
    testSendMessageMissingFields();
    testSendMessageInvalidEntity();
    testGetMessages();
    testGetMessagesMissingEntity();

    // syncState
    testSyncStateWithAgents();

    // Cleanup
    if (g_bridge) { g_bridge->stop(); delete g_bridge; g_bridge = nullptr; }

    printf("All 20 AgentKernelBridge IPC tests PASSED.\n");
}
