// Tests for LLM HttpClient + LLMClient — all use mock/stub responses (no real API calls).
#include "llm/HttpClient.h"
#include "llm/LLMClient.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace LLM;

// ─── HTTP stub tests ────────────────────────────────────────────────────────

static void testHttpClientStubReturnsMock() {
    HttpClient::setMockResponse(200, R"({"ok":true})");
    auto resp = HttpClient::post("http://localhost/test", R"({"x":1})");
    assert(resp.ok());
    assert(resp.statusCode == 200);
    assert(resp.body.find("\"ok\":true") != std::string::npos);
    printf("  PASS: testHttpClientStubReturnsMock\n");
}

static void testHttpClientStubError() {
    HttpClient::setMockResponse(500, R"({"error":"internal"})");
    auto resp = HttpClient::post("http://localhost/fail", "{}");
    assert(!resp.ok());
    assert(resp.statusCode == 500);
    printf("  PASS: testHttpClientStubError\n");
}

static void testHttpClientTimeout() {
    // Just verify setTimeout doesn't crash
    HttpClient::setTimeout(60);
    HttpClient::setTimeout(30);  // reset
    printf("  PASS: testHttpClientTimeout\n");
}

// ─── LLMClient request building ─────────────────────────────────────────────

static void testBuildRequestBodySingleMessage() {
    LLMConfig cfg;
    cfg.provider = Provider::OpenAI;
    cfg.model = "gpt-4o-mini";
    cfg.temperature = 0.5f;
    cfg.maxTokens = 256;
    LLMClient client(cfg);

    std::vector<ChatMessage> msgs = {{"user", "Hello"}};
    std::string body = client.buildRequestBody(msgs);

    // Must contain model
    assert(body.find("\"model\":\"gpt-4o-mini\"") != std::string::npos);
    // Must contain message
    assert(body.find("\"role\":\"user\"") != std::string::npos);
    assert(body.find("\"content\":\"Hello\"") != std::string::npos);
    // Must contain temperature and max_tokens
    assert(body.find("\"temperature\":0.5") != std::string::npos);
    assert(body.find("\"max_tokens\":256") != std::string::npos);

    printf("  PASS: testBuildRequestBodySingleMessage\n");
}

static void testBuildRequestBodyMultiMessage() {
    LLMConfig cfg;
    cfg.provider = Provider::OpenAI;
    cfg.model = "deepseek-chat";
    cfg.temperature = 0.7f;
    cfg.maxTokens = 1024;
    LLMClient client(cfg);

    std::vector<ChatMessage> msgs = {
        {"system", "You are a helpful assistant."},
        {"user", "What is 2+2?"},
        {"assistant", "4"},
        {"user", "And 3+3?"}
    };
    std::string body = client.buildRequestBody(msgs);

    assert(body.find("\"role\":\"system\"") != std::string::npos);
    assert(body.find("\"role\":\"assistant\"") != std::string::npos);
    assert(body.find("\"model\":\"deepseek-chat\"") != std::string::npos);

    printf("  PASS: testBuildRequestBodyMultiMessage\n");
}

static void testBuildRequestBodyEscapesSpecialChars() {
    LLMConfig cfg;
    cfg.model = "test-model";
    cfg.temperature = 0.7f;
    cfg.maxTokens = 128;
    LLMClient client(cfg);

    std::vector<ChatMessage> msgs = {{"user", "Line1\nLine2\tTab\"Quote"}};
    std::string body = client.buildRequestBody(msgs);

    // Must contain escaped characters
    assert(body.find("\\n") != std::string::npos);
    assert(body.find("\\t") != std::string::npos);
    assert(body.find("\\\"") != std::string::npos);

    printf("  PASS: testBuildRequestBodyEscapesSpecialChars\n");
}

// ─── LLMClient response parsing ─────────────────────────────────────────────

static void testParseResponseSuccess() {
    std::string json = R"({
  "id": "chatcmpl-123",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "The answer is 42."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 15,
    "completion_tokens": 8,
    "total_tokens": 23
  }
})";

    ChatResponse resp = LLMClient::parseResponse(json);
    assert(resp.ok);
    assert(resp.content == "The answer is 42.");
    assert(resp.promptTokens == 15);
    assert(resp.completionTokens == 8);
    assert(resp.error.empty());

    printf("  PASS: testParseResponseSuccess\n");
}

static void testParseResponseEmpty() {
    ChatResponse resp = LLMClient::parseResponse("");
    assert(!resp.ok);
    assert(resp.error.find("Empty") != std::string::npos);

    printf("  PASS: testParseResponseEmpty\n");
}

static void testParseResponseMalformed() {
    // JSON without "choices" key
    ChatResponse resp = LLMClient::parseResponse(R"({"foo":"bar"})");
    assert(!resp.ok);
    assert(!resp.error.empty());

    printf("  PASS: testParseResponseMalformed\n");
}

static void testParseResponseApiError() {
    std::string json = R"({
  "error": {
    "message": "Invalid API key",
    "type": "authentication_error",
    "code": "invalid_api_key"
  }
})";

    ChatResponse resp = LLMClient::parseResponse(json);
    assert(!resp.ok);
    assert(resp.error.find("Invalid API key") != std::string::npos);

    printf("  PASS: testParseResponseApiError\n");
}

// ─── Endpoint resolution ────────────────────────────────────────────────────

static void testResolveEndpointDefaults() {
    {
        LLMConfig cfg;
        cfg.provider = Provider::OpenAI;
        LLMClient c(cfg);
        assert(c.resolveEndpoint() == "https://api.openai.com/v1/chat/completions");
    }
    {
        LLMConfig cfg;
        cfg.provider = Provider::DeepSeek;
        LLMClient c(cfg);
        assert(c.resolveEndpoint() == "https://api.deepseek.com/v1/chat/completions");
    }
    {
        LLMConfig cfg;
        cfg.provider = Provider::Gemini;
        LLMClient c(cfg);
        assert(c.resolveEndpoint() ==
               "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions");
    }
    {
        LLMConfig cfg;
        cfg.provider = Provider::Custom;
        cfg.baseUrl = "http://localhost:11434/v1";
        LLMClient c(cfg);
        assert(c.resolveEndpoint() == "http://localhost:11434/v1/chat/completions");
    }

    printf("  PASS: testResolveEndpointDefaults\n");
}

// ─── Integration: LLMClient with stub HttpClient ────────────────────────────

static void testLLMClientCompleteWithStub() {
    // Set up a mock response
    HttpClient::setMockResponse(200, R"({
  "choices": [{"message": {"role": "assistant", "content": "Mock reply!"}}],
  "usage": {"prompt_tokens": 5, "completion_tokens": 3}
})");

    LLMConfig cfg;
    cfg.provider = Provider::OpenAI;
    cfg.apiKey = "sk-test";
    cfg.model = "gpt-4o-mini";
    cfg.temperature = 0.7f;
    cfg.maxTokens = 256;
    LLMClient client(cfg);

    ChatResponse resp = client.complete("You are helpful.", "Hi!");
    assert(resp.ok);
    assert(resp.content == "Mock reply!");
    assert(resp.promptTokens == 5);
    assert(resp.completionTokens == 3);

    printf("  PASS: testLLMClientCompleteWithStub\n");
}

static void testLLMClientChatMultiTurn() {
    HttpClient::setMockResponse(200, R"({
  "choices": [{"message": {"role": "assistant", "content": "Multi-turn ok"}}],
  "usage": {"prompt_tokens": 20, "completion_tokens": 5}
})");

    LLMConfig cfg;
    cfg.provider = Provider::DeepSeek;
    cfg.model = "deepseek-chat";
    cfg.maxTokens = 512;
    LLMClient client(cfg);

    std::vector<ChatMessage> msgs = {
        {"system", "Be concise."},
        {"user", "Hello"},
        {"assistant", "Hi there!"},
        {"user", "How are you?"}
    };

    ChatResponse resp = client.chat(msgs);
    assert(resp.ok);
    assert(resp.content == "Multi-turn ok");

    printf("  PASS: testLLMClientChatMultiTurn\n");
}

static void testLLMClientHttpError() {
    HttpClient::setMockResponse(401, R"({"error":{"message":"Unauthorized"}})");

    LLMConfig cfg;
    cfg.provider = Provider::OpenAI;
    cfg.apiKey = "bad-key";
    cfg.model = "gpt-4o-mini";
    LLMClient client(cfg);

    ChatResponse resp = client.complete("", "Test");
    assert(!resp.ok);
    assert(resp.error.find("401") != std::string::npos);

    printf("  PASS: testLLMClientHttpError\n");
}

// ─── TCP mock server integration test ───────────────────────────────────────

// A tiny TCP server that returns a canned HTTP response.
struct MockHttpServer {
    int listenFd = -1;
    int port = 0;
    std::atomic<bool> running{false};
    std::thread thread;

    bool start() {
        listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) return false;

        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0; // let OS pick a port

        if (::bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(listenFd);
            return false;
        }

        socklen_t len = sizeof(addr);
        getsockname(listenFd, (struct sockaddr*)&addr, &len);
        port = ntohs(addr.sin_port);

        if (::listen(listenFd, 1) < 0) {
            ::close(listenFd);
            return false;
        }

        running = true;
        thread = std::thread([this]() { serve(); });
        return true;
    }

    void serve() {
        // Accept one connection, read request, send response, close.
        while (running) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(listenFd, &fds);
            struct timeval tv { 0, 200000 }; // 200ms
            if (select(listenFd + 1, &fds, nullptr, nullptr, &tv) <= 0) continue;

            int clientFd = ::accept(listenFd, nullptr, nullptr);
            if (clientFd < 0) continue;

            // Read the request (drain it)
            char buf[4096];
            while (true) {
                ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) break;
                buf[n] = '\0';
                // Check if we've received the end of headers + body
                if (strstr(buf, "\r\n\r\n")) break;
            }

            const char* body = R"({"choices":[{"message":{"role":"assistant","content":"TCP mock reply"}}],"usage":{"prompt_tokens":7,"completion_tokens":4}})";
            char resp[1024];
            int respLen = snprintf(resp, sizeof(resp),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n%s",
                (int)strlen(body), body);
            send(clientFd, resp, respLen, 0);
            ::close(clientFd);
            break; // serve one request only
        }
    }

    void stop() {
        running = false;
        if (thread.joinable()) thread.join();
        if (listenFd >= 0) ::close(listenFd);
    }
};

static void testLLMClientWithTcpMockServer() {
    // Only test with curl if available; stub mode can't actually connect to a TCP server
    // in a meaningful way, but we test that the HttpClient::post stub still returns mock data.
    // When HAS_CURL is defined, this test exercises the real HTTP path.

#ifdef HAS_CURL
    MockHttpServer server;
    if (!server.start()) {
        printf("  SKIP: testLLMClientWithTcpMockServer (could not start TCP server)\n");
        return;
    }

    // Reset mock (this test should use real curl, not the stub mock)
    HttpClient::setMockResponse(200, "");  // Reset — curl won't use this

    std::string url = "http://127.0.0.1:" + std::to_string(server.port) + "/v1/chat/completions";

    LLMConfig cfg;
    cfg.provider = Provider::Custom;
    cfg.baseUrl = "http://127.0.0.1:" + std::to_string(server.port) + "/v1";
    cfg.model = "test-model";
    cfg.temperature = 0.7f;
    cfg.maxTokens = 256;
    LLMClient client(cfg);

    ChatResponse resp = client.complete("sys", "Hello via TCP");
    server.stop();

    assert(resp.ok);
    assert(resp.content == "TCP mock reply");
    assert(resp.promptTokens == 7);
    assert(resp.completionTokens == 4);

    printf("  PASS: testLLMClientWithTcpMockServer (real curl)\n");
#else
    // Stub mode: just verify the stub returns default mock
    HttpClient::setMockResponse(200, ""); // reset to default mock (empty body → default content)
    LLMConfig cfg;
    cfg.provider = Provider::Custom;
    cfg.baseUrl = "http://127.0.0.1:1/v1";
    cfg.model = "test";
    LLMClient client(cfg);

    ChatResponse resp = client.complete("sys", "Hello");
    // Stub returns default mock which parses as valid
    assert(resp.ok);
    assert(resp.content == "Hello from mock LLM.");

    printf("  PASS: testLLMClientWithTcpMockServer (stub mode)\n");
#endif
}

// ─── Test runner ────────────────────────────────────────────────────────────

extern void runLlmTests();

void runLlmTests() {
    printf("Running LLM tests...\n");

    // HTTP client tests
    testHttpClientStubReturnsMock();
    testHttpClientStubError();
    testHttpClientTimeout();

    // Request building
    testBuildRequestBodySingleMessage();
    testBuildRequestBodyMultiMessage();
    testBuildRequestBodyEscapesSpecialChars();

    // Response parsing
    testParseResponseSuccess();
    testParseResponseEmpty();
    testParseResponseMalformed();
    testParseResponseApiError();

    // Endpoint resolution
    testResolveEndpointDefaults();

    // Integration (stub)
    testLLMClientCompleteWithStub();
    testLLMClientChatMultiTurn();
    testLLMClientHttpError();

    // TCP mock server (real curl or stub fallback)
    testLLMClientWithTcpMockServer();

    printf("All 15 LLM tests PASSED.\n");
}
