#include "HttpClient.h"
#include <cstdio>
#include <cstring>

namespace LLM {

// Static member definitions
int HttpClient::timeoutSec_ = 30;
int HttpClient::mockStatusCode_ = 200;
std::string HttpClient::mockBody_;

// Request/response size limits (safety guardrails)
static const size_t MAX_REQUEST_SIZE  = 10 * 1024 * 1024;   // 10 MB
static const size_t MAX_RESPONSE_SIZE = 10 * 1024 * 1024;   // 10 MB

void HttpClient::setTimeout(int seconds) {
    timeoutSec_ = seconds;
}

void HttpClient::setMockResponse(int statusCode, const std::string& body) {
    mockStatusCode_ = statusCode;
    mockBody_ = body;
}

// ─────────────────────────────────────────────────────────────────────────────
// curl implementation (compiled when HAS_CURL is defined by CMake)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef HAS_CURL

#include <curl/curl.h>

namespace {
    struct WriteContext {
        std::string* out;
        size_t max_size;
    };

    // Callback: append received data to a std::string, with size limit.
    size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        auto* ctx = static_cast<WriteContext*>(userdata);
        size_t total = size * nmemb;
        if (ctx->out->size() + total > ctx->max_size) {
            return 0;  // Abort — response too large
        }
        ctx->out->append(ptr, total);
        return total;
    }
}

HttpResponse HttpClient::post(const std::string& url,
                              const std::string& jsonBody,
                              const std::vector<std::pair<std::string,std::string>>& headers) {
    HttpResponse resp;

    // Guard: reject oversized requests
    if (jsonBody.size() > MAX_REQUEST_SIZE) {
        resp.statusCode = 413;
        resp.body = "Request body exceeds maximum size (10 MB)";
        return resp;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.statusCode = 0;
        resp.body = "curl_easy_init() failed";
        return resp;
    }

    std::string responseBuffer;
    WriteContext ctx{&responseBuffer, MAX_RESPONSE_SIZE};

    struct curl_slist* chunk = nullptr;
    chunk = curl_slist_append(chunk, "Content-Type: application/json");
    for (auto& h : headers) {
        std::string hdr = h.first + ": " + h.second;
        chunk = curl_slist_append(chunk, hdr.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)jsonBody.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeoutSec_);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)timeoutSec_);

    // Connection reuse: enable TCP keepalive
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L);

    // Thread safety: don't use signals (important for multi-threaded servers)
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // Follow redirects (up to 3)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        resp.statusCode = (int)code;
        resp.body = responseBuffer;
    } else if (res == CURLE_WRITE_ERROR) {
        resp.statusCode = 502;
        resp.body = "Response exceeds maximum size (10 MB)";
    } else {
        resp.statusCode = 0;
        resp.body = std::string("curl error: ") + curl_easy_strerror(res);
    }

    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    return resp;
}

#else  // ─── Stub implementation ───────────────────────────────────────────────

HttpClient HttpClient_instance_;  // suppress unused warning in some compilers

HttpResponse HttpClient::post(const std::string& url,
                              const std::string& /*jsonBody*/,
                              const std::vector<std::pair<std::string,std::string>>& /*headers*/) {
    // Log to stderr so tests can optionally verify the call happened.
    std::fprintf(stderr, "[HttpClient::stub] POST %s\n", url.c_str());

    HttpResponse resp;
    resp.statusCode = mockStatusCode_;
    if (!mockBody_.empty()) {
        resp.body = mockBody_;
    } else {
        // Default mock: a well-formed OpenAI chat completion response.
        resp.body = R"({
  "id": "mock-chatcmpl-001",
  "object": "chat.completion",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Hello from mock LLM."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 10,
    "completion_tokens": 5,
    "total_tokens": 15
  }
})";
    }
    return resp;
}

#endif // HAS_CURL

} // namespace LLM
