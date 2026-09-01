// Tests for DecisionEngine — agent state + LLM → action plan.
#include "llm/DecisionEngine.h"
#include "llm/HttpClient.h"
#include "agent_kernel.h"
#include "ipc/AgentKernelBridge.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace ECS;
using namespace LLM;

// Helper: build a mock OpenAI response with given assistant content.
static std::string makeMockLLMResponse(const std::string& assistantContent) {
    // Escape the content for JSON embedding
    std::string escaped;
    for (char c : assistantContent) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:   escaped += c;      break;
        }
    }
    return R"({"choices":[{"message":{"role":"assistant","content":")" + escaped + R"("}}],"usage":{"prompt_tokens":10,"completion_tokens":5}})";
}

// Helper: create a test entity with identity + personality.
static EntityId createTestEntity(Registry& reg, const std::string& name) {
    Entity e = reg.createEntity();
    EntityId id = e.getId();
    auto& ident = reg.addComponent<IdentityComponent>(id,
        "agent-" + std::to_string(id), name, AgentRole::Specialist);
    ident.department  = "engineering";
    ident.companyRole = "backend_dev";
    ident.teamId      = "team-alpha";

    reg.addComponent<PersonalityComponent>(id,
        80.0f, 30.0f, 60.0f, 40.0f, 55.0f, 70.0f);

    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    skills.getSkill("backend_dev")->xp = 800;

    reg.addComponent<CareerComponent>(id);
    return id;
}

// ─── Test: parseDecision with valid JSON ─────────────────────────────────────

static void testParseDecisionValidJson() {
    std::string llmResponse = R"({"action": "execute", "reasoning": "I have the skills", "confidence": 0.9})";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    assert(d.action == Action::Execute);
    assert(d.reasoning == "I have the skills");
    assert(std::abs(d.confidence - 0.9f) < 0.01f);
    assert(d.delegateTo.empty());
    assert(d.details.empty());

    printf("  PASS: testParseDecisionValidJson\n");
}

// ─── Test: parseDecision with delegate action ────────────────────────────────

static void testParseDecisionDelegate() {
    std::string llmResponse = R"({"action": "delegate", "reasoning": "Not my domain", "confidence": 0.7, "delegateTo": "frontend-team", "details": "Need CSS expertise"})";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    assert(d.action == Action::Delegate);
    assert(d.reasoning == "Not my domain");
    assert(std::abs(d.confidence - 0.7f) < 0.01f);
    assert(d.delegateTo == "frontend-team");
    assert(d.details == "Need CSS expertise");

    printf("  PASS: testParseDecisionDelegate\n");
}

// ─── Test: parseDecision with malformed text → default ──────────────────────

static void testParseDecisionMalformedText() {
    std::string llmResponse = "I think we should just proceed with the task because it looks straightforward.";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    // Should fall back to default
    assert(d.action == Action::Execute);
    assert(std::abs(d.confidence - 0.5f) < 0.01f);
    assert(d.reasoning == llmResponse);

    printf("  PASS: testParseDecisionMalformedText\n");
}

// ─── Test: parseDecision with partial JSON (missing confidence) ─────────────

static void testParseDecisionPartialJson() {
    std::string llmResponse = R"({"action": "reflect", "reasoning": "Need more context"})";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    assert(d.action == Action::Reflect);
    assert(d.reasoning == "Need more context");
    assert(std::abs(d.confidence - 0.5f) < 0.01f); // default

    printf("  PASS: testParseDecisionPartialJson\n");
}

// ─── Test: parseDecision with requestInfo action ─────────────────────────────

static void testParseDecisionRequestInfo() {
    std::string llmResponse = R"({"action": "requestInfo", "reasoning": "Which framework?", "confidence": 0.6})";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    assert(d.action == Action::RequestInfo);
    assert(d.reasoning == "Which framework?");

    printf("  PASS: testParseDecisionRequestInfo\n");
}

// ─── Test: parseDecision with decline action ─────────────────────────────────

static void testParseDecisionDecline() {
    std::string llmResponse = R"({"action": "decline", "reasoning": "Security risk", "confidence": 0.95})";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    assert(d.action == Action::Decline);
    assert(d.reasoning == "Security risk");
    assert(std::abs(d.confidence - 0.95f) < 0.01f);

    printf("  PASS: testParseDecisionDecline\n");
}

// ─── Test: parseDecision confidence clamping ─────────────────────────────────

static void testParseDecisionConfidenceClamping() {
    {
        std::string llmResponse = R"({"action": "execute", "reasoning": "ok", "confidence": 1.5})";
        Decision d = DecisionEngine::parseDecision(llmResponse);
        assert(std::abs(d.confidence - 1.0f) < 0.01f);
    }
    {
        std::string llmResponse = R"({"action": "execute", "reasoning": "ok", "confidence": -0.3})";
        Decision d = DecisionEngine::parseDecision(llmResponse);
        assert(std::abs(d.confidence - 0.0f) < 0.01f);
    }

    printf("  PASS: testParseDecisionConfidenceClamping\n");
}

// ─── Test: parseDecision with JSON embedded in text ──────────────────────────

static void testParseDecisionEmbeddedJson() {
    // LLM often wraps JSON in explanatory text
    std::string llmResponse = "Based on my analysis, here is my decision:\n\n"
        R"({"action": "execute", "reasoning": "Good match", "confidence": 0.85})"
        "\n\nI believe this is the best course of action.";

    Decision d = DecisionEngine::parseDecision(llmResponse);

    assert(d.action == Action::Execute);
    assert(d.reasoning == "Good match");
    assert(std::abs(d.confidence - 0.85f) < 0.01f);

    printf("  PASS: testParseDecisionEmbeddedJson\n");
}

// ─── Test: Decision::toJson ──────────────────────────────────────────────────

static void testDecisionToJson() {
    Decision d;
    d.action = Action::Delegate;
    d.reasoning = "Frontend task";
    d.confidence = 0.75f;
    d.delegateTo = "css-team";
    d.details = "Use Tailwind";

    std::string json = d.toJson();

    assert(json.find("\"action\":\"delegate\"") != std::string::npos);
    assert(json.find("\"reasoning\":\"Frontend task\"") != std::string::npos);
    assert(json.find("\"confidence\":0.75") != std::string::npos);
    assert(json.find("\"delegateTo\":\"css-team\"") != std::string::npos);
    assert(json.find("\"details\":\"Use Tailwind\"") != std::string::npos);

    printf("  PASS: testDecisionToJson\n");
}

static void testDecisionToJsonExecute() {
    Decision d;
    d.action = Action::Execute;
    d.reasoning = "All good";
    d.confidence = 1.0f;

    std::string json = d.toJson();

    assert(json.find("\"action\":\"execute\"") != std::string::npos);
    // delegateTo and details should be omitted when empty
    assert(json.find("delegateTo") == std::string::npos);
    assert(json.find("details") == std::string::npos);

    printf("  PASS: testDecisionToJsonExecute\n");
}

// ─── Test: actionToString / actionFromString roundtrip ───────────────────────

static void testActionRoundtrip() {
    struct { Action a; const char* s; } cases[] = {
        {Action::Execute,     "execute"},
        {Action::Delegate,    "delegate"},
        {Action::RequestInfo, "requestInfo"},
        {Action::Decline,     "decline"},
        {Action::Reflect,     "reflect"},
    };

    for (const auto& c : cases) {
        std::string s = DecisionEngine::actionToString(c.a);
        assert(s == c.s);
        Action a = DecisionEngine::actionFromString(s);
        assert(a == c.a);
    }

    // Test case-insensitive parsing
    assert(DecisionEngine::actionFromString("EXECUTE") == Action::Execute);
    assert(DecisionEngine::actionFromString("Delegate") == Action::Delegate);
    assert(DecisionEngine::actionFromString("RequestInfo") == Action::RequestInfo);
    assert(DecisionEngine::actionFromString("DECLINE") == Action::Decline);
    assert(DecisionEngine::actionFromString("Reflect") == Action::Reflect);

    // Test unknown → default
    assert(DecisionEngine::actionFromString("unknown") == Action::Execute);

    printf("  PASS: testActionRoundtrip\n");
}

// ─── Test: decide() with mock LLM ───────────────────────────────────────────

static void testDecideWithMockLLM() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();
    EntityId id = createTestEntity(reg, "测试工程师");

    // Set up mock LLM response
    HttpClient::setMockResponse(200, makeMockLLMResponse(
        R"({"action": "execute", "reasoning": "I have the skills", "confidence": 0.9})"));

    LLMConfig cfg;
    cfg.provider = Provider::Custom;
    cfg.baseUrl  = "http://localhost:1/v1";
    cfg.model    = "test-model";
    LLMClient client(cfg);

    DecisionEngine engine(&client);
    Decision d = engine.decide(reg, id, "review this code");

    assert(d.action == Action::Execute);
    assert(d.reasoning == "I have the skills");
    assert(std::abs(d.confidence - 0.9f) < 0.01f);

    printf("  PASS: testDecideWithMockLLM\n");
}

// ─── Test: decide() with LLM error → fallback ───────────────────────────────

static void testDecideWithLLMError() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();
    EntityId id = createTestEntity(reg, "失败测试");

    // Mock a failed HTTP response
    HttpClient::setMockResponse(500, R"({"error":{"message":"Internal server error"}})");

    LLMConfig cfg;
    cfg.provider = Provider::Custom;
    cfg.baseUrl  = "http://localhost:1/v1";
    cfg.model    = "test-model";
    LLMClient client(cfg);

    DecisionEngine engine(&client);
    Decision d = engine.decide(reg, id, "do something");

    // Should fallback to RequestInfo with low confidence
    assert(d.action == Action::RequestInfo);
    assert(std::abs(d.confidence - 0.3f) < 0.01f);
    assert(d.reasoning.find("LLM unavailable") != std::string::npos);

    printf("  PASS: testDecideWithLLMError\n");
}

// ─── Test: decideBatch with 2 tasks ─────────────────────────────────────────

static void testDecideBatch() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();
    EntityId id = createTestEntity(reg, "批量测试");

    // Mock responses: first task → execute, second task → decline
    // Since the stub returns the same mock for each call, both should get the same result
    HttpClient::setMockResponse(200, makeMockLLMResponse(
        R"({"action": "execute", "reasoning": "Ready", "confidence": 0.8})"));

    LLMConfig cfg;
    cfg.provider = Provider::Custom;
    cfg.baseUrl  = "http://localhost:1/v1";
    cfg.model    = "test-model";
    LLMClient client(cfg);

    DecisionEngine engine(&client);
    std::vector<std::string> tasks = {"review code", "deploy service"};
    auto results = engine.decideBatch(reg, id, tasks);

    assert(results.size() == 2);
    assert(results[0].action == Action::Execute);
    assert(results[1].action == Action::Execute);
    assert(std::abs(results[0].confidence - 0.8f) < 0.01f);
    assert(std::abs(results[1].confidence - 0.8f) < 0.01f);

    printf("  PASS: testDecideBatch\n");
}

// ─── Test: IPC agentDecide endpoint ─────────────────────────────────────────

static const char* TEST_DECIDE_SOCKET = "/tmp/agent-kernel-decide-test.sock";

class DecideTestClient {
public:
    ~DecideTestClient() { close(); }

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
        char chunk[4096];
        while (true) {
            ssize_t n = ::recv(fd_, chunk, sizeof(chunk) - 1, 0);
            if (n <= 0) break;
            chunk[n] = '\0';
            buffer += chunk;
            // Look for newline delimiter
            auto pos = buffer.find('\n');
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

static void testIPCAgentDecide() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();
    EntityId id = createTestEntity(reg, "IPC测试员");

    // Set up mock LLM response
    HttpClient::setMockResponse(200, makeMockLLMResponse(
        R"({"action": "execute", "reasoning": "I have the skills", "confidence": 0.9})"));

    // Clean up any leftover socket
    unlink(TEST_DECIDE_SOCKET);

    IPC::AgentKernelBridge bridge(TEST_DECIDE_SOCKET);
    assert(bridge.start());

    DecideTestClient client;
    assert(client.connect(TEST_DECIDE_SOCKET));

    // Send agentDecide request
    std::string request = R"({"method":"agentDecide","params":{"entityId":)" +
                          std::to_string(id) +
                          R"(,"task":"review this code"}})";
    assert(client.send(request));

    // Receive response
    std::string response = client.recv();
    client.close();
    bridge.stop();
    unlink(TEST_DECIDE_SOCKET);

    // Verify response contains decision data
    assert(response.find("\"ok\":true") != std::string::npos);
    assert(response.find("\"action\":\"execute\"") != std::string::npos);
    assert(response.find("\"confidence\"") != std::string::npos);

    printf("  PASS: testIPCAgentDecide\n");
}

static void testIPCAgentDecideMissingTask() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();
    EntityId id = createTestEntity(reg, "错误测试");

    unlink(TEST_DECIDE_SOCKET);

    IPC::AgentKernelBridge bridge(TEST_DECIDE_SOCKET);
    assert(bridge.start());

    DecideTestClient client;
    assert(client.connect(TEST_DECIDE_SOCKET));

    // Send request without task
    std::string request = R"({"method":"agentDecide","params":{"entityId":)" +
                          std::to_string(id) + R"(}})";
    assert(client.send(request));

    std::string response = client.recv();
    client.close();
    bridge.stop();
    unlink(TEST_DECIDE_SOCKET);

    // Should get an error
    assert(response.find("\"ok\":false") != std::string::npos);
    assert(response.find("task is required") != std::string::npos);

    printf("  PASS: testIPCAgentDecideMissingTask\n");
}

static void testIPCAgentDecideEntityNotFound() {
    Registry::getInstance().clear();

    unlink(TEST_DECIDE_SOCKET);

    IPC::AgentKernelBridge bridge(TEST_DECIDE_SOCKET);
    assert(bridge.start());

    DecideTestClient client;
    assert(client.connect(TEST_DECIDE_SOCKET));

    // Send request with non-existent entity
    std::string request = R"({"method":"agentDecide","params":{"entityId":9999,"task":"do something"}})";
    assert(client.send(request));

    std::string response = client.recv();
    client.close();
    bridge.stop();
    unlink(TEST_DECIDE_SOCKET);

    // Should get an error
    assert(response.find("\"ok\":false") != std::string::npos);
    assert(response.find("entity not found") != std::string::npos);

    printf("  PASS: testIPCAgentDecideEntityNotFound\n");
}

// ─── Test runner ────────────────────────────────────────────────────────────

extern void runDecisionEngineTests();

void runDecisionEngineTests() {
    printf("Running DecisionEngine tests...\n");

    // parseDecision tests
    testParseDecisionValidJson();
    testParseDecisionDelegate();
    testParseDecisionMalformedText();
    testParseDecisionPartialJson();
    testParseDecisionRequestInfo();
    testParseDecisionDecline();
    testParseDecisionConfidenceClamping();
    testParseDecisionEmbeddedJson();

    // toJson tests
    testDecisionToJson();
    testDecisionToJsonExecute();

    // action mapping tests
    testActionRoundtrip();

    // Integration tests (with mock LLM)
    testDecideWithMockLLM();
    testDecideWithLLMError();
    testDecideBatch();

    // IPC tests
    testIPCAgentDecide();
    testIPCAgentDecideMissingTask();
    testIPCAgentDecideEntityNotFound();

    printf("All 17 DecisionEngine tests PASSED.\n");
}
