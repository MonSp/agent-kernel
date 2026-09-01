#include "SimulationRunner.h"
#include <sstream>

namespace Systems {

SimulationRunner::SimulationRunner(TickEngine* engine) : engine_(engine) {}

std::vector<TickResult> SimulationRunner::run(
    ECS::Registry& reg,
    const std::vector<ECS::EntityId>& entities,
    int ticks,
    const std::vector<std::string>& tasks) {

    std::vector<TickResult> allResults;
    allResults.reserve(entities.size() * static_cast<size_t>(ticks));

    for (int t = 0; t < ticks; ++t) {
        for (size_t i = 0; i < entities.size(); ++i) {
            const std::string& task = tasks.empty() ? "default task" : tasks[i % tasks.size()];
            TickResult result = engine_->tick(reg, entities[i], task);
            allResults.push_back(std::move(result));
        }
    }

    return allResults;
}

SimulationSummary SimulationRunner::summarize(const std::vector<TickResult>& results) {
    SimulationSummary summary;
    summary.totalTicks = static_cast<int>(results.size());
    float totalConfidence = 0.0f;

    for (const auto& r : results) {
        std::string key = actionTypeToString(r.action);
        summary.actionCounts[key]++;
        totalConfidence += r.decision.confidence;
    }

    if (!results.empty()) {
        summary.averageConfidence = totalConfidence / static_cast<float>(results.size());
    }

    return summary;
}

std::string SimulationSummary::toJson() const {
    std::ostringstream oss;
    oss << "{\"totalTicks\":" << totalTicks;
    oss << ",\"averageConfidence\":" << averageConfidence;
    oss << ",\"actionCounts\":{";
    bool first = true;
    for (const auto& [key, count] : actionCounts) {
        if (!first) oss << ",";
        oss << "\"" << key << "\":" << count;
        first = false;
    }
    oss << "}}";
    return oss.str();
}

} // namespace Systems
