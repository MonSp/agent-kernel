#pragma once

#include "../Component.h"
#include <cstdint>

enum class CareerStage : uint8_t {
    Junior = 0,
    Mid = 1,
    Senior = 2,
    Lead = 3,
    Expert = 4
};

struct CareerComponent : public ECS::ComponentBase<CareerComponent> {
    uint32_t totalXp;
    CareerStage stage;
    uint32_t tasksCompleted;
    uint32_t tasksSucceeded;
    float avgReviewScore;

    CareerComponent()
        : totalXp(0), stage(CareerStage::Junior),
          tasksCompleted(0), tasksSucceeded(0), avgReviewScore(0.0f) {}

    // Add XP and auto-promote if threshold reached
    void addXp(uint32_t amount) {
        totalXp += amount;
        while (canPromote()) {
            stage = static_cast<CareerStage>(
                static_cast<uint8_t>(stage) + 1);
        }
    }

    // Promotion thresholds: Junior>=500, Mid>=2000, Senior>=5000, Lead>=10000
    bool canPromote() const {
        if (stage == CareerStage::Expert) return false;
        switch (stage) {
            case CareerStage::Junior: return totalXp >= 500;
            case CareerStage::Mid:    return totalXp >= 2000;
            case CareerStage::Senior: return totalXp >= 5000;
            case CareerStage::Lead:   return totalXp >= 10000;
            default: return false;
        }
    }

    float getSuccessRate() const {
        if (tasksCompleted == 0) return 0.0f;
        return static_cast<float>(tasksSucceeded) /
               static_cast<float>(tasksCompleted);
    }
};
