#pragma once
// Multi-provider LLM client — OpenAI-compatible chat completions.
// Supports OpenAI, DeepSeek, Gemini (via OpenAI-compatible endpoint), Custom.

#include <string>
#include <vector>
#include <cstdint>

namespace LLM {

enum class Provider : uint8_t {
    OpenAI   = 0,
    DeepSeek = 1,
    Gemini   = 2,
    Custom   = 3
};

struct LLMConfig {
    Provider   provider    = Provider::OpenAI;
    std::string apiKey;
    std::string baseUrl;       // e.g. "https://api.openai.com/v1"
    std::string model;         // e.g. "gpt-4o-mini" or "deepseek-chat"
    float       temperature   = 0.7f;
    int         maxTokens     = 1024;
};

struct ChatMessage {
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

struct ChatResponse {
    std::string content;          // assistant's reply
    int         promptTokens     = 0;
    int         completionTokens = 0;
    bool        ok               = false;
    std::string error;
};

class LLMClient {
public:
    explicit LLMClient(LLMConfig config);

    // Single chat completion (synchronous).
    ChatResponse chat(const std::vector<ChatMessage>& messages);

    // Convenience: single system + user prompt → assistant response.
    ChatResponse complete(const std::string& systemPrompt,
                          const std::string& userPrompt);

    // ── Helpers exposed for testing ──────────────────────────────────────
    // Build the request JSON body (OpenAI-compatible format).
    std::string buildRequestBody(const std::vector<ChatMessage>& messages) const;

    // Parse an OpenAI-compatible JSON response into ChatResponse.
    static ChatResponse parseResponse(const std::string& json);

    // Resolve the full chat-completion endpoint URL.
    std::string resolveEndpoint() const;

private:
    LLMConfig config_;
};

} // namespace LLM
