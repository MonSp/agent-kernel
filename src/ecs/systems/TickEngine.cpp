#include "TickEngine.h"
#include <chrono>
#include <sstream>

namespace {

std::string escapeJsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
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
    return out;
}

} // anonymous namespace

namespace Systems {

TickEngine::TickEngine(LLM::LLMClient* client) : engine_(client) {}

int TickEngine::nextTickNumber() {
    return tickCounter_++;
}

TickResult TickEngine::tick(ECS::Registry& reg, ECS::EntityId id, const std::string& task) {
    TickResult result;
    result.tickNumber = nextTickNumber();

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    result.timestamp = static_cast<uint64_t>(ms);

    result.decision = engine_.decide(reg, id, task);
    result.action = mapDecisionToAction(result.decision, reg, id);
    result.effects = generateEffects(result.action, result.decision, reg, id);
    ActionExecutor::apply(reg, id, result.effects);

    return result;
}

std::string TickResult::toJson() const {
    std::ostringstream oss;
    oss << "{\"action\":\"" << actionTypeToString(action) << "\"";
    oss << ",\"tickNumber\":" << tickNumber;
    oss << ",\"timestamp\":" << timestamp;
    oss << ",\"decision\":" << decision.toJson();
    oss << ",\"effects\":[";
    for (size_t i = 0; i < effects.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"target\":" << static_cast<int>(effects[i].target);
        oss << ",\"fieldName\":\"" << escapeJsonStr(effects[i].fieldName) << "\"";
        oss << ",\"delta\":" << effects[i].delta;
        oss << ",\"description\":\"" << escapeJsonStr(effects[i].description) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

} // namespace Systems
