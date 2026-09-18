// Tests for HttpClient — request/response limits, mock behavior, error handling.
#include "llm/HttpClient.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace LLM;

// ─── Mock response behavior ──────────────────────────────────────────────────

static void testMockResponseCustom() {
    HttpClient::setMockResponse(201, R"({"custom":"value"})");
    auto resp = HttpClient::post("http://test", "{}");
    assert(resp.statusCode == 201);
    assert(resp.body.find("\"custom\"") != std::string::npos);
    printf("  PASS: testMockResponseCustom\n");
}

static void testMockResponseReset() {
    // Set a custom mock, then reset to default
    HttpClient::setMockResponse(500, "error");
    auto resp1 = HttpClient::post("http://test", "{}");
    assert(resp1.statusCode == 500);

    HttpClient::setMockResponse(200, "");
    auto resp2 = HttpClient::post("http://test", "{}");
    assert(resp2.statusCode == 200);
    // Empty mock body → default OpenAI mock
    assert(resp2.body.find("chat.completion") != std::string::npos);
    printf("  PASS: testMockResponseReset\n");
}

static void testMockResponseEmptyBody() {
    HttpClient::setMockResponse(204, "");
    auto resp = HttpClient::post("http://test", "{}");
    assert(resp.statusCode == 204);
    assert(resp.ok());
    printf("  PASS: testMockResponseEmptyBody\n");
}

// ─── HttpResponse helper ─────────────────────────────────────────────────────

static void testHttpResponseOk() {
    HttpClient::setMockResponse(200, "ok");
    auto resp = HttpClient::post("http://test", "{}");
    assert(resp.ok());

    HttpClient::setMockResponse(299, "ok");
    resp = HttpClient::post("http://test", "{}");
    assert(resp.ok());

    HttpClient::setMockResponse(300, "redirect");
    resp = HttpClient::post("http://test", "{}");
    assert(!resp.ok());

    HttpClient::setMockResponse(404, "not found");
    resp = HttpClient::post("http://test", "{}");
    assert(!resp.ok());

    HttpClient::setMockResponse(500, "server error");
    resp = HttpClient::post("http://test", "{}");
    assert(!resp.ok());

    printf("  PASS: testHttpResponseOk\n");
}

static void testHttpResponseZeroStatus() {
    // statusCode 0 should not be ok()
    HttpClient::setMockResponse(0, "connection failed");
    auto resp = HttpClient::post("http://test", "{}");
    assert(!resp.ok());
    printf("  PASS: testHttpResponseZeroStatus\n");
}

// ─── Request handling ────────────────────────────────────────────────────────

static void testPostEmptyBody() {
    HttpClient::setMockResponse(200, R"({"ok":true})");
    auto resp = HttpClient::post("http://test", "");
    assert(resp.ok());
    printf("  PASS: testPostEmptyBody\n");
}

static void testPostWithHeaders() {
    HttpClient::setMockResponse(200, R"({"ok":true})");
    std::vector<std::pair<std::string,std::string>> headers = {
        {"Authorization", "Bearer test-key"},
        {"X-Custom", "value"},
    };
    auto resp = HttpClient::post("http://test", "{}", headers);
    assert(resp.ok());
    printf("  PASS: testPostWithHeaders\n");
}

static void testPostLargeBody() {
    // Stub mode doesn't enforce size limits, but should handle large body gracefully
    HttpClient::setMockResponse(200, R"({"ok":true})");
    std::string largeBody(100000, 'x');
    auto resp = HttpClient::post("http://test", largeBody);
    assert(resp.ok());
    printf("  PASS: testPostLargeBody\n");
}

// ─── Timeout configuration ───────────────────────────────────────────────────

static void testSetTimeout() {
    // Just verify it doesn't crash
    HttpClient::setTimeout(5);
    HttpClient::setTimeout(30);
    HttpClient::setTimeout(0);
    HttpClient::setTimeout(-1);  // Edge case
    printf("  PASS: testSetTimeout (no crash)\n");
}

// ─── Multiple sequential requests ────────────────────────────────────────────

static void testSequentialRequests() {
    for (int i = 0; i < 5; ++i) {
        HttpClient::setMockResponse(200, R"({"seq":)" + std::to_string(i) + "}");
        auto resp = HttpClient::post("http://test", "{}");
        assert(resp.ok());
        assert(resp.body.find(std::to_string(i)) != std::string::npos);
    }
    printf("  PASS: testSequentialRequests\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runHttpClientTests() {
    printf("Running HttpClient unit tests...\n");

    testMockResponseCustom();
    testMockResponseReset();
    testMockResponseEmptyBody();
    testHttpResponseOk();
    testHttpResponseZeroStatus();
    testPostEmptyBody();
    testPostWithHeaders();
    testPostLargeBody();
    testSetTimeout();
    testSequentialRequests();

    printf("All 10 HttpClient unit tests PASSED.\n");
}
