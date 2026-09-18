#include "DecisionEngine.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace LLM {

// ─── Shared utilities ───────────────────────────────────────────────────────

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

// Shared JSON string escaping — handles quotes, backslashes, control chars
void appendJsonEscaped(std::string& out, const std::string& s) {
    out.reserve(out.size() + s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
}

// Find a JSON object in the text (first '{' ... matching '}')
// Returns the substring, or empty if not found.
std::string extractJsonObject(const std::string& text) {
    size_t start = text.find('{');
    if (start == std::string::npos) return "";

    int depth = 0;
    for (size_t i = start; i < text.size(); ++i) {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}') {
            --depth;
            if (depth == 0) {
                return text.substr(start, i - start + 1);
            }
        }
    }
    return ""; // unmatched braces
}

// Extract a string value for a key: "key":"value"
std::string findString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;

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

// Extract a float value for a key: "key": 0.85
float findFloat(const std::string& json, const std::string& key, float defaultVal) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    if (pos >= json.size()) return defaultVal;

    // Parse number (possibly negative, possibly with decimal)
    size_t start = pos;
    if (pos < json.size() && json[pos] == '-') ++pos;
    while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '.')) ++pos;

    if (pos == start) return defaultVal;
    try {
        return std::stof(json.substr(start, pos - start));
    } catch (...) {
        return defaultVal;
    }
}

} // anonymous namespace

// ─── Action ↔ string mapping ────────────────────────────────────────────────

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
    appendJsonEscaped(out, reasoning);
    out += "\",\"confidence\":" + std::to_string(confidence);
    if (!delegateTo.empty()) {
        out += ",\"delegateTo\":\"";
        appendJsonEscaped(out, delegateTo);
        out += "\"";
    }
    if (!details.empty()) {
        out += ",\"details\":\"";
        appendJsonEscaped(out, details);
        out += "\"";
    }
    out += "}";
    return out;
}

// ─── DecisionEngine::parseDecision ──────────────────────────────────────────

Decision DecisionEngine::parseDecision(const std::string& llmResponse) {
    Decision d;

    // Try to extract a JSON object from the response
    std::string json = extractJsonObject(llmResponse);
    if (json.empty()) {
        // No JSON found — return default with raw response as reasoning
        d.action = Action::Execute;
        d.confidence = 0.5f;
        d.reasoning = llmResponse;
        return d;
    }

    // Parse action
    std::string actionStr = findString(json, "action");
    if (!actionStr.empty()) {
        d.action = actionFromString(actionStr);
    }

    // Parse reasoning
    d.reasoning = findString(json, "reasoning");

    // Parse confidence
    d.confidence = findFloat(json, "confidence", 0.5f);
    // Clamp to [0, 1]
    if (d.confidence < 0.0f) d.confidence = 0.0f;
    if (d.confidence > 1.0f) d.confidence = 1.0f;

    // Parse optional fields
    d.delegateTo = findString(json, "delegateTo");
    d.details    = findString(json, "details");

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
