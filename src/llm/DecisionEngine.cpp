#include "DecisionEngine.h"
#include "JsonUtils.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace LLM {

// ─── Action ↔ string mapping ────────────────────────────────────────────────

namespace {
// O(1) lookup table for string → Action mapping
const std::unordered_map<std::string, Action>& actionLookupTable() {
    static const std::unordered_map<std::string, Action> table = {
        {"execute",      Action::Execute},
        {"delegate",     Action::Delegate},
        {"requestinfo",  Action::RequestInfo},
        {"request_info", Action::RequestInfo},
        {"request info", Action::RequestInfo},
        {"decline",      Action::Decline},
        {"reflect",      Action::Reflect},
    };
    return table;
}
} // anonymous namespace

Action DecisionEngine::actionFromString(const std::string& s) {
    // Normalize to lowercase for lookup
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = actionLookupTable().find(lower);
    return (it != actionLookupTable().end()) ? it->second : Action::Execute; // default fallback
}

std::string DecisionEngine::actionToString(Action a) {
    switch (a) {
        case Action::Execute:     return "execute";
        case Action::Delegate:    return "delegate";
        case Action::RequestInfo: return "requestInfo";
        case Action::Decline:     return "decline";
        case Action::Reflect:     return "reflect";
        default:                  return "execute";
    }
}

// ─── Decision::toJson ───────────────────────────────────────────────────────

std::string Decision::toJson() const {
    std::string out;
    out.reserve(256);
    out += "{\"action\":\"";
    out += DecisionEngine::actionToString(action);
    out += "\",\"reasoning\":\"";
    // Escape reasoning for JSON
    JsonUtils::appendEscaped(out, reasoning);
    out += "\",\"confidence\":" + std::to_string(confidence);
    if (!delegateTo.empty()) {
        out += ",\"delegateTo\":\"";
        JsonUtils::appendEscaped(out, delegateTo);
        out += "\"";
    }
    if (!details.empty()) {
        out += ",\"details\":\"";
        JsonUtils::appendEscaped(out, details);
        out += "\"";
    }
    out += "}";
    return out;
}

// ─── DecisionEngine::parseDecision ──────────────────────────────────────────

Decision DecisionEngine::parseDecision(const std::string& llmResponse) {
    Decision d;

    // Try to extract a JSON object from the response
    std::string json = JsonUtils::extractObject(llmResponse);
    if (json.empty()) {
        // No JSON found — return default with raw response as reasoning
        d.action = Action::Execute;
        d.confidence = 0.5f;
        d.reasoning = llmResponse;
        return d;
    }

    // Parse action
    std::string actionStr = JsonUtils::findString(json, "action");
    if (!actionStr.empty()) {
        d.action = actionFromString(actionStr);
    }

    // Parse reasoning
    d.reasoning = JsonUtils::findString(json, "reasoning");

    // Parse confidence
    d.confidence = JsonUtils::findFloat(json, "confidence", 0.5f);
    // Clamp to [0, 1]
    if (d.confidence < 0.0f) d.confidence = 0.0f;
    if (d.confidence > 1.0f) d.confidence = 1.0f;

    // Parse optional fields
    d.delegateTo = JsonUtils::findString(json, "delegateTo");
    d.details    = JsonUtils::findString(json, "details");

    return d;
}

// ─── DecisionEngine ─────────────────────────────────────────────────────────

DecisionEngine::DecisionEngine(LLMClient* client) : client_(client) {}

Decision DecisionEngine::decide(ECS::Registry& registry, ECS::EntityId entityId,
                                 const std::string& task) {
    // 1. Build messages from entity state
    std::vector<ChatMessage> messages = PromptBuilder::buildMessages(registry, entityId, task);

    // 2. Call LLM
    ChatResponse resp = client_->chat(messages);

    // 3. Parse response
    if (!resp.ok) {
        // LLM call failed — return a conservative default
        Decision d;
        d.action = Action::RequestInfo;
        d.confidence = 0.3f;
        d.reasoning = "LLM unavailable: " + resp.error;
        return d;
    }

    return parseDecision(resp.content);
}

std::vector<Decision> DecisionEngine::decideBatch(ECS::Registry& registry,
                                                    ECS::EntityId entityId,
                                                    const std::vector<std::string>& tasks) {
    std::vector<Decision> results;
    results.reserve(tasks.size());
    for (const auto& task : tasks) {
        results.push_back(decide(registry, entityId, task));
    }
    return results;
}

} // namespace LLM
