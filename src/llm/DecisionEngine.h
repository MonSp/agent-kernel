#pragma once
// DecisionEngine — agent state + LLM → structured action plan.
// Combines PromptBuilder (state → prompt) and LLMClient (prompt → response),
// then parses the LLM's JSON reply into a typed Decision.

#include "PromptBuilder.h"
#include "LLMClient.h"
#include "../ecs/Registry.h"
#include <string>
#include <vector>
#include <cstdint>

namespace LLM {

enum class Action : uint8_t {
    Execute = 0,    // proceed with the task
    Delegate,       // assign to another agent
    RequestInfo,    // need more information
    Decline,        // cannot/should not do this
    Reflect         // need to think more
};

struct Decision {
    Action action       = Action::Execute;
    std::string reasoning;
    float confidence    = 0.5f;     // 0.0 - 1.0
    std::string delegateTo;         // if Delegate, target agent/role
    std::string details;            // additional structured data

    // Serialize to JSON for IPC
    std::string toJson() const;
};

class DecisionEngine {
public:
    explicit DecisionEngine(LLMClient* client);

    // Make a decision for an agent on a given task.
    // Builds prompts from entity state, calls LLM, parses response.
    Decision decide(ECS::Registry& registry, ECS::EntityId entityId, const std::string& task);

    // Batch: make decisions for multiple tasks (one LLM call per task).
    std::vector<Decision> decideBatch(ECS::Registry& registry, ECS::EntityId entityId,
                                       const std::vector<std::string>& tasks);

    // Parse a raw LLM response into a Decision (static, testable).
    static Decision parseDecision(const std::string& llmResponse);

    // Map an action string (lowercase) to an Action enum value.
    static Action actionFromString(const std::string& s);

    // Map an Action enum value to a string.
    static std::string actionToString(Action a);

private:
    LLMClient* client_;
};

} // namespace LLM
