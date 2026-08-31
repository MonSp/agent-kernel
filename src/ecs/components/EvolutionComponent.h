#pragma once

#include "../Component.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct EvolutionRecord {
    std::string ruleId;
    float effectivenessBefore;
    float effectivenessAfter;
    uint64_t timestamp;

    EvolutionRecord()
        : effectivenessBefore(0.0f), effectivenessAfter(0.0f), timestamp(0) {}

    EvolutionRecord(const std::string& rid, float before, float after, uint64_t ts)
        : ruleId(rid), effectivenessBefore(before),
          effectivenessAfter(after), timestamp(ts) {}
};

struct EvolutionComponent : public ECS::ComponentBase<EvolutionComponent> {
    std::vector<EvolutionRecord> history;
    uint32_t totalEvolutions;
    uint32_t successfulEvolutions;
    float diversityScore;  // 0.0-1.0

    static constexpr uint32_t MAX_EVOLUTIONS_PER_RULE = 3;

    EvolutionComponent()
        : totalEvolutions(0), successfulEvolutions(0), diversityScore(0.0f) {}

    void recordEvolution(const std::string& ruleId, float before,
                         float after, uint64_t timestamp) {
        history.emplace_back(ruleId, before, after, timestamp);
        totalEvolutions++;
        if (after > before) {
            successfulEvolutions++;
        }
    }

    float getSuccessRate() const {
        if (totalEvolutions == 0) return 0.0f;
        return static_cast<float>(successfulEvolutions) /
               static_cast<float>(totalEvolutions);
    }

    // Returns true if we can still apply this rule (max 3 evolutions per ruleId)
    bool shouldEvolve(const std::string& ruleId) const {
        uint32_t count = 0;
        for (const auto& record : history) {
            if (record.ruleId == ruleId) {
                count++;
            }
        }
        return count < MAX_EVOLUTIONS_PER_RULE;
    }
};
