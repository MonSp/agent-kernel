#include "LLMClient.h"
#include "HttpClient.h"
#include "JsonUtils.h"
#include <cstring>

namespace LLM {

// ─── JSON parsing helpers (thin wrappers over JsonUtils) ─────────────────────

namespace {

// Find the "usage" object in JSON and extract prompt_tokens / completion_tokens.
void findUsageTokens(const std::string& json, int& promptTokens, int& completionTokens) {
    promptTokens = 0;
    completionTokens = 0;

    auto usagePos = json.find("\"usage\"");
    if (usagePos == std::string::npos) return;

    // Search within a window after "usage"
    std::string usageBlock = json.substr(usagePos);
    promptTokens     = JsonUtils::findInt(usageBlock, "prompt_tokens");
    completionTokens = JsonUtils::findInt(usageBlock, "completion_tokens");
}

// Extract content from the first choice in "choices" array.
// Looks for the pattern: "choices" → first element → "message" → "content"
std::string extractChoiceContent(const std::string& json) {
    // Find "choices"
    auto choicesPos = json.find("\"choices\"");
    if (choicesPos == std::string::npos) return "";

    // Find first '[' after "choices"
    auto arrStart = json.find('[', choicesPos);
    if (arrStart == std::string::npos) return "";

    // Find the "content" value within the first choice block.
    auto contentKeyPos = json.find("\"content\"", arrStart);
    if (contentKeyPos == std::string::npos) return "";

    return JsonUtils::findString(json.substr(contentKeyPos), "content");
}

} // anonymous namespace

// ─── LLMClient implementation ──────────────────────────────────────────────

LLMClient::LLMClient(LLMConfig config) : config_(std::move(config)) {}

std::string LLMClient::resolveEndpoint() const {
    // Derive base URL from provider if not explicitly set.
    std::string base = config_.baseUrl;
    if (base.empty()) {
        switch (config_.provider) {
            case Provider::OpenAI:   base = "https://api.openai.com/v1"; break;
            case Provider::DeepSeek: base = "https://api.deepseek.com/v1"; break;
            case Provider::Gemini:   base = "https://generativelanguage.googleapis.com/v1beta/openai"; break;
            case Provider::Custom:   base = "http://localhost:8080/v1"; break;
        }
    }
    // Ensure no trailing slash
    if (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/chat/completions";
}

std::string LLMClient::buildRequestBody(const std::vector<ChatMessage>& messages) const {
    std::string out;
    out.reserve(256 + messages.size() * 128);
    out += "{\"model\":\"";
    JsonUtils::appendEscaped(out, config_.model);
    out += "\",\"messages\":[";

    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) out += ",";
        out += "{\"role\":\"";
        JsonUtils::appendEscaped(out, messages[i].role);
        out += "\",\"content\":\"";
        JsonUtils::appendEscaped(out, messages[i].content);
        out += "\"}";
    }

    out += "],\"temperature\":" + std::to_string(config_.temperature);
    out += ",\"max_tokens\":" + std::to_string(config_.maxTokens);
    out += "}";
    return out;
}

ChatResponse LLMClient::parseResponse(const std::string& json) {
    ChatResponse resp;

    if (json.empty()) {
        resp.error = "Empty response body";
        return resp;
    }

    // Check for API error response (OpenAI format: {"error": {"message": "...", ...}})
    auto errorPos = json.find("\"error\"");
    if (errorPos != std::string::npos) {
        // Try to extract error message
        auto msgPos = json.find("\"message\"", errorPos);
        if (msgPos != std::string::npos) {
            resp.error = JsonUtils::findString(json.substr(msgPos), "message");
        }
        if (resp.error.empty()) {
            resp.error = "API error (could not parse error message)";
        }
        return resp;
    }

    // Extract choice content
    resp.content = extractChoiceContent(json);

    // Extract usage tokens
    findUsageTokens(json, resp.promptTokens, resp.completionTokens);

    resp.ok = !resp.content.empty();
    if (!resp.ok) {
        resp.error = "No content in response choices";
    }

    return resp;
}

ChatResponse LLMClient::chat(const std::vector<ChatMessage>& messages) {
    std::string endpoint = resolveEndpoint();
    std::string body     = buildRequestBody(messages);

    // Build auth header
    std::vector<std::pair<std::string,std::string>> headers;
    if (!config_.apiKey.empty()) {
        headers.emplace_back("Authorization", "Bearer " + config_.apiKey);
    }

    HttpResponse httpResp = HttpClient::post(endpoint, body, headers);

    if (!httpResp.ok()) {
        ChatResponse resp;
        resp.error = "HTTP " + std::to_string(httpResp.statusCode) + ": " + httpResp.body;
        return resp;
    }

    return parseResponse(httpResp.body);
}

ChatResponse LLMClient::complete(const std::string& systemPrompt,
                                 const std::string& userPrompt) {
    std::vector<ChatMessage> messages;
    if (!systemPrompt.empty()) {
        messages.push_back({"system", systemPrompt});
    }
    messages.push_back({"user", userPrompt});
    return chat(messages);
}

} // namespace LLM
