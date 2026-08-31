#pragma once

#include "../Component.h"
#include <cstdint>
#include <optional>

enum class AgentLifeState : uint8_t {
    Idle = 0,
    Active = 1,
    Paused = 2,
    Terminated = 3
};

enum class BirthType : uint8_t {
    Natural = 0,
    WarOrphan = 1,
    Wanderer = 2,
    DemonBeast = 3
};

enum class DeathCause : uint8_t {
    AgeLimit = 0,
    Battle = 1,
    CultivationFail = 2,
    MonsterAttack = 3,
    Robbery = 4,
    LawEnforcement = 5,
    TribulationFail = 6,
    FireDeviation = 7
};

struct LifecycleComponent : public ECS::ComponentBase<LifecycleComponent> {
    uint64_t birthTime;
    float age;
    AgentLifeState lifeState;
    BirthType birthType;
    std::optional<DeathCause> deathCause;
    uint64_t lastUpdateTime;

    LifecycleComponent() : birthTime(0), age(0), lifeState(AgentLifeState::Active),
        birthType(BirthType::Natural), deathCause(std::nullopt), lastUpdateTime(0) {}

    void ageOneYear() {
        age += 1.0f;
    }

    void setDead(DeathCause cause) {
        lifeState = AgentLifeState::Terminated;
        deathCause = cause;
    }

    bool isTerminated() const {
        return lifeState == AgentLifeState::Terminated;
    }

    bool shouldBeDeadByAge(uint32_t maxAge) const {
        return age >= maxAge;
    }
};
