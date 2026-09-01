#include "LLMClient.h"
#include "HttpClient.h"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace LLM {

// ─── Minimal JSON helpers (no external dependency) ──────────────────────────

namespace {

// Escape a string for embedding in a JSON string literal.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Find the value of a top-level JSON key whose value is a string.
// Returns empty string if not found.  Handles escaped quotes inside value.
std::string findJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";

    // skip past key and any whitespace/colon
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos; // skip opening quote

    std::string val;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\' && pos < json.size()) {
            char esc = json[pos++];
            switch (esc) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                case 'b':  val += '\b'; break;
                case 'f':  val += '\f'; break;
                default:   val += esc;  break;
            }
        } else if (c == '"') {
            break;
        } else {
            val += c;
        }
    }
    return val;
}

// Find the integer value of a top-level JSON key.
int findJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    // parse optional minus + digits
    bool neg = false;
    if (pos < json.size() && json[pos] == '-') { neg = true; ++pos; }
    int val = 0;
    bool foundDigit = false;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        ++pos;
        foundDigit = true;
    }
    if (!foundDigit) return defaultVal;
    return neg ? -val : val;
}

// Find the "usage" object in JSON and extract prompt_tokens / completion_tokens.
// We search for the usage block directly.
void findUsageTokens(const std::string& json, int& promptTokens, int& completionTokens) {
    promptTokens = 0;
    completionTokens = 0;

    auto usagePos = json.find("\"usage\"");
    if (usagePos == std::string::npos) return;

    // Search within a window after "usage"
    std::string usageBlock = json.substr(usagePos);
    promptTokens     = findJsonInt(usageBlock, "prompt_tokens");
    completionTokens = findJsonInt(usageBlock, "completion_tokens");
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
    // We scan from arrStart forward looking for "content".
    auto contentKeyPos = json.find("\"content\"", arrStart);
    if (contentKeyPos == std::string::npos) return "";

    // Check we haven't jumped past the first array element (a rough heuristic:
    // look for the closing of the first object '}' before contentKeyPos — if there
    // are two objects, we'd be in the wrong one. For simplicity, just take the
    // first "content" after the array start.)
    return findJsonString(json.substr(contentKeyPos), "content");
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
    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << jsonEscape(config_.model) << "\",";
    oss << "\"messages\":[";

    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"role\":\"" << jsonEscape(messages[i].role) << "\","
            << "\"content\":\"" << jsonEscape(messages[i].content) << "\"}";
    }

    oss << "],";
    oss << "\"temperature\":" << config_.temperature << ",";
    oss << "\"max_tokens\":" << config_.maxTokens;
    oss << "}";

    return oss.str();
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
            resp.error = findJsonString(json.substr(msgPos), "message");
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
