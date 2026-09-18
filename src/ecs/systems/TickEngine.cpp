#include "TickEngine.h"
#include "../../llm/JsonUtils.h"
#include <chrono>
#include <string>

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
    // Serialize decision once — reuse for both reserve() and concatenation
    const std::string decisionJson = decision.toJson();

    // Pre-reserve capacity: base + ~80 bytes per effect + decision JSON
    std::string out;
    out.reserve(128 + effects.size() * 80 + decisionJson.size());

    out += "{\"action\":\"";
    out += actionTypeToString(action);
    out += "\",\"tickNumber\":" + std::to_string(tickNumber);
    out += ",\"timestamp\":" + std::to_string(timestamp);
    out += ",\"decision\":" + decisionJson;
    out += ",\"effects\":[";

    for (size_t i = 0; i < effects.size(); ++i) {
        if (i > 0) out += ",";
        out += "{\"target\":" + std::to_string(static_cast<int>(effects[i].target));
        out += ",\"fieldName\":\"";
        JsonUtils::appendEscaped(out, effects[i].fieldName);
        out += "\",\"delta\":" + std::to_string(effects[i].delta);
        out += ",\"description\":\"";
        JsonUtils::appendEscaped(out, effects[i].description);
        out += "\"}";
    }
    out += "]}";
    return out;
}

} // namespace Systems
