#pragma once
// LLM HTTP Client — minimal POST-JSON → JSON-response client for agent-kernel.
// Uses libcurl when available; otherwise returns mock responses (stub mode).

#include <string>
#include <vector>
#include <utility>

namespace LLM {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    bool ok() const { return statusCode >= 200 && statusCode < 300; }
};

class HttpClient {
public:
    // POST JSON to URL, return response.
    static HttpResponse post(const std::string& url,
                             const std::string& jsonBody,
                             const std::vector<std::pair<std::string,std::string>>& headers = {});

    // Set global timeout in seconds.
    static void setTimeout(int seconds);

    // For stub/testing: inject a mock response that the stub will return.
    static void setMockResponse(int statusCode, const std::string& body);

private:
    static int timeoutSec_;
    static int mockStatusCode_;
    static std::string mockBody_;
};

} // namespace LLM
