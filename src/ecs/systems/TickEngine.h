#pragma once
// TickEngine — single-agent tick: perceive → decide → act → record.

#include "ActionTypes.h"
#include "ActionEffect.h"
#include "ActionExecutor.h"
#include "../../llm/DecisionEngine.h"
#include "../../llm/LLMClient.h"
#include "../Registry.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Systems {

struct TickResult {
    ActionType action = ActionType::ExecuteTask;
    LLM::Decision decision;
    std::vector<ActionEffect> effects;
    uint64_t timestamp = 0;
    int tickNumber = 0;

    std::string toJson() const;
};

class TickEngine {
public:
    explicit TickEngine(LLM::LLMClient* client);

    TickResult tick(ECS::Registry& reg, ECS::EntityId id, const std::string& task);
    int nextTickNumber();

private:
    LLM::DecisionEngine engine_;
    int tickCounter_ = 0;
};

} // namespace Systems
