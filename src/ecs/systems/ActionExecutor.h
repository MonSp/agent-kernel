#pragma once
// ActionExecutor — applies ActionEffect lists to ECS entities.

#include "ActionEffect.h"
#include "../Registry.h"
#include <vector>

namespace Systems {

class ActionExecutor {
public:
    static void apply(ECS::Registry& reg, ECS::EntityId id,
                      const std::vector<ActionEffect>& effects);
};

} // namespace Systems
