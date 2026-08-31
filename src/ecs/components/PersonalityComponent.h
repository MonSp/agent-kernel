#pragma once

#include "../Component.h"

struct PersonalityComponent : public ECS::ComponentBase<PersonalityComponent> {
    float ambition;
    float caution;
    float loyalty;
    float greed;
    float sociability;
    float diligence;

    PersonalityComponent() : ambition(50.0f), caution(50.0f), loyalty(50.0f),
        greed(50.0f), sociability(50.0f), diligence(50.0f) {}

    PersonalityComponent(float amb, float cau, float loy, float gre,
                         float soc, float dil)
        : ambition(amb), caution(cau), loyalty(loy), greed(gre),
          sociability(soc), diligence(dil) {}

    float getOverall() const {
        return (ambition + caution + loyalty + greed + sociability + diligence) / 6.0f;
    }

    bool isAggressive() const {
        return ambition > 70.0f && greed > 50.0f;
    }

    bool isCautious() const {
        return caution > 70.0f;
    }

    bool isLoyal() const {
        return loyalty > 70.0f;
    }

    bool isSocial() const {
        return sociability > 60.0f;
    }

    bool isDiligent() const {
        return diligence > 60.0f;
    }
};
