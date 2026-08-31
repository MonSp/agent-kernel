#pragma once

#include "../Component.h"
#include <cmath>
#include <cstdint>

enum class EmotionType : uint8_t {
    Anger = 0,
    Fear = 1,
    Joy = 2
};

#pragma pack(push, 1)
struct EmotionCooldown {
    uint32_t targetSlot = 0;
    EmotionType emotionType = EmotionType::Anger;
    uint8_t triggerType = 0;
    uint64_t cooldownUntilFrame = 0;
};
#pragma pack(pop)

struct SocialComponent : public ECS::ComponentBase<SocialComponent> {
    float hunger;
    float fatigue;
    float energy;
    float socialDesire;
    float mood;

    float anger;
    float fear;
    float joy;

    static constexpr size_t MAX_COOLDOWNS = 16;
    static constexpr float HIGH_FEAR_THRESHOLD = 60.0f;
    static constexpr float HIGH_ANGER_THRESHOLD = 60.0f;
    static constexpr float HIGH_JOY_THRESHOLD = 50.0f;
    EmotionCooldown emotionCooldowns[MAX_COOLDOWNS] = {};
    uint8_t cooldownCount;

    SocialComponent() : hunger(0.0f), fatigue(0.0f), energy(80.0f),
        socialDesire(30.0f), mood(60.0f),
        anger(0.0f), fear(0.0f), joy(0.0f), cooldownCount(0) {}

    void tickDaily(float deltaHours) {
        hunger = clamp100(hunger + 4.0f * deltaHours);
        fatigue = clamp100(fatigue + 3.0f * deltaHours);
        energy = clamp100(energy - 3.0f * deltaHours);
        socialDesire = clamp100(socialDesire + 2.0f * deltaHours);
        mood = clamp100(mood - 2.0f * deltaHours);
    }

    void onEat() {
        hunger = clamp100(hunger - 40.0f);
        mood = clamp100(mood + 10.0f);
        energy = clamp100(energy + 10.0f);
    }

    void onSleep() {
        fatigue = clamp100(fatigue - 50.0f);
        energy = clamp100(energy + 40.0f);
    }

    void onRest(float deltaHours) {
        fatigue = clamp100(fatigue - 5.0f * deltaHours);
        energy = clamp100(energy + 8.0f * deltaHours);
    }

    void onSocialize() {
        socialDesire = clamp100(socialDesire - 25.0f);
        mood = clamp100(mood + 15.0f);
    }

    bool isHungry() const { return hunger > 70.0f; }
    bool isExhausted() const { return fatigue > 80.0f; }
    bool wantsSocial() const { return socialDesire > 60.0f; }

    void tickEmotions(float deltaTime) {
        float decayPerFrame = 0.995f;
        float effectiveFrames = deltaTime * 60.0f;
        if (effectiveFrames > 10.0f) effectiveFrames = 10.0f;
        float decayFactor = std::pow(decayPerFrame, effectiveFrames);
        anger *= decayFactor;
        fear *= decayFactor;
        joy *= decayFactor;
        clampEmotions();
    }

    void addAnger(float amount) { anger = clamp100(anger + amount); }
    void addFear(float amount)  { fear = clamp100(fear + amount); }
    void addJoy(float amount)   { joy = clamp100(joy + amount); }

    bool isEnraged(float caution) const {
        float threshold = 70.0f - caution * 0.3f;
        return anger > threshold;
    }
    bool isTerrified() const { return fear > HIGH_FEAR_THRESHOLD; }
    bool isElated(float sociability) const {
        float threshold = 80.0f - sociability * 0.2f;
        return joy > threshold;
    }

    void onInsulted(float caution) {
        float modifier = 1.0f - (caution / 100.0f) * 0.5f;
        addAnger(20.0f * modifier);
    }
    void onAttacked(float damage, float caution) {
        (void)damage;
        addFear(30.0f);
        float angerMod = 1.0f - (caution / 100.0f) * 0.3f;
        addAnger(15.0f * angerMod);
    }
    void onGiftReceived() { addJoy(25.0f); }
    void onSocialSuccess() { addJoy(10.0f); }

    bool isInCooldown(uint32_t targetSlot, EmotionType type, uint8_t trigger, uint64_t currentFrame) const {
        for (uint8_t i = 0; i < cooldownCount; i++) {
            if (emotionCooldowns[i].targetSlot == targetSlot &&
                emotionCooldowns[i].emotionType == type &&
                emotionCooldowns[i].triggerType == trigger &&
                emotionCooldowns[i].cooldownUntilFrame > currentFrame) {
                return true;
            }
        }
        return false;
    }

    void addCooldown(uint32_t targetSlot, EmotionType type, uint8_t trigger, uint64_t currentFrame) {
        for (uint8_t i = 0; i < cooldownCount; i++) {
            if (emotionCooldowns[i].cooldownUntilFrame <= currentFrame) {
                emotionCooldowns[i].targetSlot = targetSlot;
                emotionCooldowns[i].emotionType = type;
                emotionCooldowns[i].triggerType = trigger;
                emotionCooldowns[i].cooldownUntilFrame = currentFrame + 72;
                return;
            }
        }

        if (cooldownCount < MAX_COOLDOWNS) {
            uint8_t idx = cooldownCount++;
            emotionCooldowns[idx].targetSlot = targetSlot;
            emotionCooldowns[idx].emotionType = type;
            emotionCooldowns[idx].triggerType = trigger;
            emotionCooldowns[idx].cooldownUntilFrame = currentFrame + 72;
            return;
        }

        uint8_t lruIdx = 0;
        uint64_t minFrame = emotionCooldowns[0].cooldownUntilFrame;
        for (uint8_t i = 1; i < cooldownCount; i++) {
            if (emotionCooldowns[i].cooldownUntilFrame < minFrame) {
                minFrame = emotionCooldowns[i].cooldownUntilFrame;
                lruIdx = i;
            }
        }
        emotionCooldowns[lruIdx].targetSlot = targetSlot;
        emotionCooldowns[lruIdx].emotionType = type;
        emotionCooldowns[lruIdx].triggerType = trigger;
        emotionCooldowns[lruIdx].cooldownUntilFrame = currentFrame + 72;
    }

    void cleanupExpiredCooldowns(uint64_t currentFrame) {
        uint8_t writeIdx = 0;
        for (uint8_t i = 0; i < cooldownCount; i++) {
            if (emotionCooldowns[i].cooldownUntilFrame > currentFrame) {
                if (writeIdx != i) emotionCooldowns[writeIdx] = emotionCooldowns[i];
                writeIdx++;
            }
        }
        cooldownCount = writeIdx;
    }

private:
    static float clamp100(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 100.0f) return 100.0f;
        return v;
    }

    void clampEmotions() {
        if (anger < 0.0f) anger = 0.0f;
        if (fear < 0.0f) fear = 0.0f;
        if (joy < 0.0f) joy = 0.0f;
        if (anger > 100.0f) anger = 100.0f;
        if (fear > 100.0f) fear = 100.0f;
        if (joy > 100.0f) joy = 100.0f;
    }
};
