#pragma once
// ActionEffect — field-level component mutations generated from ActionTypes.

#include "ActionTypes.h"
#include "../Registry.h"
#include "../components/SkillTreeComponent.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>

namespace Systems {

enum class TargetComponent : uint8_t {
    Social = 0,    // SocialComponent: energy, mood, socialDesire
    SkillTree,     // SkillTreeComponent: skill XP
    Career,        // CareerComponent: career XP
    Personality,   // PersonalityComponent: trait drift
    Memory         // MemoryRingComponent: milestone entry
};

struct ActionEffect {
    TargetComponent target;
    std::string fieldName;   // e.g. "energy", "programming", "ambition"
    float delta = 0.0f;      // numeric change
    std::string stringValue; // for Memory entries
    std::string description; // human-readable reason
};

// Pick a random skill name from the entity's SkillTreeComponent.
inline std::string pickRandomSkill(ECS::Registry& reg, ECS::EntityId id) {
    auto* skills = reg.getComponent<SkillTreeComponent>(id);
    if (!skills || skills->skills.empty()) return "";
    size_t idx = static_cast<size_t>(std::rand()) % skills->skills.size();
    auto it = skills->skills.begin();
    std::advance(it, idx);
    return it->first;
}

// Pick a random personality trait name.
inline std::string pickRandomTrait() {
    static const char* traits[] = {"ambition", "caution", "loyalty", "greed", "sociability", "diligence"};
    return traits[static_cast<size_t>(std::rand()) % 6];
}

// Generate deterministic effects for a given action type.
inline std::vector<ActionEffect> generateEffects(ActionType type,
                                                  const LLM::Decision& d,
                                                  ECS::Registry& reg,
                                                  ECS::EntityId id) {
    std::vector<ActionEffect> effects;

    switch (type) {
        case ActionType::ExecuteTask: {
            float xp = 50.0f + d.confidence * 150.0f;
            float careerXp = 10.0f + d.confidence * 40.0f;
            std::string skill = pickRandomSkill(reg, id);
            if (!skill.empty()) {
                effects.push_back({TargetComponent::SkillTree, skill, xp, "", "task completion"});
            }
            effects.push_back({TargetComponent::Career, "totalXp", careerXp, "", "career progress"});
            std::string desc = d.details.empty() ? d.reasoning : d.details;
            effects.push_back({TargetComponent::Memory, "milestone", 0.0f,
                               "Completed task: " + desc, "experience recorded"});
            break;
        }
        case ActionType::PracticeSkill: {
            std::string skill = pickRandomSkill(reg, id);
            if (!skill.empty()) {
                effects.push_back({TargetComponent::SkillTree, skill, 100.0f, "", "focused practice"});
            }
            effects.push_back({TargetComponent::Social, "energy", -10.0f, "", "practice fatigue"});
            break;
        }
        case ActionType::Delegate: {
            effects.push_back({TargetComponent::Memory, "interaction", 0.0f,
                               "Delegated task to: " + d.delegateTo, "delegation"});
            break;
        }
        case ActionType::Rest: {
            effects.push_back({TargetComponent::Social, "energy", 30.0f, "", "rest recovery"});
            effects.push_back({TargetComponent::Social, "mood", 10.0f, "", "rest relaxation"});
            break;
        }
        case ActionType::Socialize: {
            effects.push_back({TargetComponent::Social, "socialDesire", -25.0f, "", "social interaction"});
            effects.push_back({TargetComponent::Social, "mood", 15.0f, "", "social bonding"});
            effects.push_back({TargetComponent::Personality, "sociability", 2.0f, "", "social growth"});
            break;
        }
        case ActionType::Study: {
            std::string skill = pickRandomSkill(reg, id);
            if (!skill.empty()) {
                effects.push_back({TargetComponent::SkillTree, skill, 80.0f, "", "study session"});
            }
            effects.push_back({TargetComponent::Social, "energy", -15.0f, "", "study fatigue"});
            break;
        }
        case ActionType::Reflect: {
            std::string trait = pickRandomTrait();
            float direction = (std::rand() % 2 == 0) ? 3.0f : -3.0f;
            effects.push_back({TargetComponent::Personality, trait, direction, "", "self-reflection"});
            effects.push_back({TargetComponent::Memory, "milestone", 0.0f,
                               "Reflected on: " + d.reasoning, "introspection"});
            break;
        }
    }

    return effects;
}

} // namespace Systems
