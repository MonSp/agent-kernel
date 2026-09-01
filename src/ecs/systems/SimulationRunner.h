#pragma once
// SimulationRunner — multi-agent N-tick batch simulation.

#include "TickEngine.h"
#include "../Registry.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Systems {

struct SimulationSummary {
    int totalTicks = 0;
    std::unordered_map<std::string, int> actionCounts;
    float averageConfidence = 0.0f;

    std::string toJson() const;
};

class SimulationRunner {
public:
    explicit SimulationRunner(TickEngine* engine);

    std::vector<TickResult> run(ECS::Registry& reg,
                                const std::vector<ECS::EntityId>& entities,
                                int ticks,
                                const std::vector<std::string>& tasks);

    static SimulationSummary summarize(const std::vector<TickResult>& results);

private:
    TickEngine* engine_;
};

} // namespace Systems
