#include "ActionExecutor.h"
#include "../components/SocialComponent.h"
#include "../components/SkillTreeComponent.h"
#include "../components/CareerComponent.h"
#include "../components/PersonalityComponent.h"
#include "../components/MemoryRingComponent.h"
#include <algorithm>
#include <cmath>

namespace Systems {

namespace {

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

void applySocial(ECS::Registry& reg, ECS::EntityId id, const ActionEffect& eff) {
    auto* social = reg.getComponent<SocialComponent>(id);
    if (!social) return;
    if (eff.fieldName == "energy") {
        social->energy = clampf(social->energy + eff.delta, 0.0f, 100.0f);
    } else if (eff.fieldName == "mood") {
        social->mood = clampf(social->mood + eff.delta, 0.0f, 100.0f);
    } else if (eff.fieldName == "socialDesire") {
        social->socialDesire = clampf(social->socialDesire + eff.delta, 0.0f, 100.0f);
    } else if (eff.fieldName == "fatigue") {
        social->fatigue = clampf(social->fatigue + eff.delta, 0.0f, 100.0f);
    } else if (eff.fieldName == "hunger") {
        social->hunger = clampf(social->hunger + eff.delta, 0.0f, 100.0f);
    }
}

void applySkillTree(ECS::Registry& reg, ECS::EntityId id, const ActionEffect& eff) {
    auto* skills = reg.getComponent<SkillTreeComponent>(id);
    if (!skills) return;
    uint32_t amount = static_cast<uint32_t>(std::max(0.0f, eff.delta));
    if (skills->hasSkill(eff.fieldName)) {
        skills->addXp(eff.fieldName, amount);
    } else {
        skills->addSkill(eff.fieldName, SkillCategory::Engineering, SkillLevel::Beginner);
        skills->addXp(eff.fieldName, amount);
    }
}

void applyCareer(ECS::Registry& reg, ECS::EntityId id, const ActionEffect& eff) {
    auto* career = reg.getComponent<CareerComponent>(id);
    if (!career) return;
    if (eff.fieldName == "totalXp") {
        uint32_t amount = static_cast<uint32_t>(std::max(0.0f, eff.delta));
        career->addXp(amount);
    }
}

void applyPersonality(ECS::Registry& reg, ECS::EntityId id, const ActionEffect& eff) {
    auto* personality = reg.getComponent<PersonalityComponent>(id);
    if (!personality) return;
    float* trait = nullptr;
    if (eff.fieldName == "ambition") trait = &personality->ambition;
    else if (eff.fieldName == "caution") trait = &personality->caution;
    else if (eff.fieldName == "loyalty") trait = &personality->loyalty;
    else if (eff.fieldName == "greed") trait = &personality->greed;
    else if (eff.fieldName == "sociability") trait = &personality->sociability;
    else if (eff.fieldName == "diligence") trait = &personality->diligence;
    if (trait) {
        *trait = clampf(*trait + eff.delta, 0.0f, 100.0f);
    }
}

void applyMemory(ECS::Registry& reg, ECS::EntityId id, const ActionEffect& eff) {
    auto* memory = reg.getComponent<MemoryRingComponent>(id);
    if (!memory) return;
    MilestoneType type = MilestoneType::MajorCommand;
    if (eff.fieldName == "interaction") {
        type = MilestoneType::DaoCompanionBond;
    }
    memory->recordMilestone(type, 0, 5);
}

} // anonymous namespace

void ActionExecutor::apply(ECS::Registry& reg, ECS::EntityId id,
                           const std::vector<ActionEffect>& effects) {
    for (const auto& eff : effects) {
        switch (eff.target) {
            case TargetComponent::Social:      applySocial(reg, id, eff); break;
            case TargetComponent::SkillTree:   applySkillTree(reg, id, eff); break;
            case TargetComponent::Career:      applyCareer(reg, id, eff); break;
            case TargetComponent::Personality: applyPersonality(reg, id, eff); break;
            case TargetComponent::Memory:      applyMemory(reg, id, eff); break;
        }
    }
}

} // namespace Systems
