// Tests for IPC agentTick + runSimulation endpoints.
#include "ipc/AgentKernelBridge.h"
#include "llm/HttpClient.h"
#include "ecs/Registry.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static void mockLLM(const std::string& content) {
    std::string escaped = escapeJson(content);
    std::string body = R"({"choices":[{"message":{"role":"assistant","content":")" + escaped + R"("}}],"usage":{"prompt_tokens":10,"completion_tokens":5}})";
    LLM::HttpClient::setMockResponse(200, body);
}

static std::string sendIPC(const std::string& socketPath, const std::string& request) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "";
    }

    std::string req = request + "\n";
    send(sock, req.c_str(), req.size(), 0);

    char buf[16384] = {0};
    std::string response;
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        response += std::string(buf, n);
        if (response.find('\n') != std::string::npos) break;
    }

    close(sock);
    return response;
}

static ECS::EntityId createFullEntity() {
    auto& reg = ECS::Registry::getInstance();
    auto e = reg.createEntity();
    auto id = e.getId();
    reg.addComponent<IdentityComponent>(id, "a" + std::to_string(id), "Test", AgentRole::Specialist);
    auto& social = reg.addComponent<SocialComponent>(id);
    social.energy = 70.0f;
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    reg.addComponent<CareerComponent>(id);
    reg.addComponent<PersonalityComponent>(id);
    reg.addComponent<MemoryRingComponent>(id);
    return id;
}

static void testAgentTickValid() {
    mockLLM(R"({"action":"execute","reasoning":"ok","confidence":0.8})");

    const char* sockPath = "/tmp/test_ipc_tick_v3.sock";
    unlink(sockPath);

    ECS::Registry::getInstance().clear();
    IPC::AgentKernelBridge bridge(sockPath);
    auto id = createFullEntity();

    bridge.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string req = R"({"method":"agentTick","params":{"entityId":)" + std::to_string(id) + R"(,"task":"test task"}})";
    std::string resp = sendIPC(sockPath, req);

    assert(resp.find("executeTask") != std::string::npos || resp.find("execute") != std::string::npos);
    assert(resp.find("tickNumber") != std::string::npos);

    bridge.stop();
    printf("  PASS: testAgentTickValid\n");
}

static void testAgentTickInvalidEntity() {
    const char* sockPath = "/tmp/test_ipc_tick_inv3.sock";
    unlink(sockPath);

    ECS::Registry::getInstance().clear();
    IPC::AgentKernelBridge bridge(sockPath);

    bridge.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string req = R"({"method":"agentTick","params":{"entityId":99999,"task":"test"}})";
    std::string resp = sendIPC(sockPath, req);

    assert(resp.find("error") != std::string::npos);

    bridge.stop();
    printf("  PASS: testAgentTickInvalidEntity\n");
}

void runIpcTickTests() {
    printf("=== test_ipc_tick ===\n");
    testAgentTickValid();
    testAgentTickInvalidEntity();
    printf("All 2 IPC tick tests PASSED.\n");
}
