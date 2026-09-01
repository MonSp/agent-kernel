#pragma once
// ActionTypes — 7 concrete agent action types + Decision → ActionType mapping.

#include "../../llm/DecisionEngine.h"
#include "../Registry.h"
#include "../components/SocialComponent.h"
#include <string>
#include <algorithm>
#include <cctype>

namespace Systems {

enum class ActionType : uint8_t {
    ExecuteTask = 0,
    PracticeSkill,
    Delegate,
    Rest,
    Socialize,
    Study,
    Reflect
};

inline std::string actionTypeToString(ActionType t) {
    switch (t) {
        case ActionType::ExecuteTask:   return "executeTask";
        case ActionType::PracticeSkill: return "practiceSkill";
        case ActionType::Delegate:      return "delegate";
        case ActionType::Rest:          return "rest";
        case ActionType::Socialize:     return "socialize";
        case ActionType::Study:         return "study";
        case ActionType::Reflect:       return "reflect";
        default:                        return "executeTask";
    }
}

inline ActionType actionTypeFromString(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "executetask")   return ActionType::ExecuteTask;
    if (lower == "practiceskill") return ActionType::PracticeSkill;
    if (lower == "delegate")      return ActionType::Delegate;
    if (lower == "rest")          return ActionType::Rest;
    if (lower == "socialize")     return ActionType::Socialize;
    if (lower == "study")         return ActionType::Study;
    if (lower == "reflect")       return ActionType::Reflect;
    return ActionType::ExecuteTask;
}

// Map a Decision + entity state → ActionType.
inline ActionType mapDecisionToAction(const LLM::Decision& d,
                                      ECS::Registry& reg,
                                      ECS::EntityId id) {
    // Rule 1: Delegate
    if (d.action == LLM::Action::Delegate) {
        return ActionType::Delegate;
    }

    // Rule 2: Reflect → Study or Reflect
    if (d.action == LLM::Action::Reflect) {
        std::string lower = d.details + " " + d.reasoning;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower.find("study") != std::string::npos ||
            lower.find("learn") != std::string::npos ||
            lower.find("research") != std::string::npos) {
            return ActionType::Study;
        }
        return ActionType::Reflect;
    }

    // Rule 3: Low energy → Rest (check SocialComponent)
    auto* social = reg.getComponent<SocialComponent>(id);
    if (social && social->energy < 30.0f) {
        return ActionType::Rest;
    }

    // Rule 4: Details mention skill → PracticeSkill
    {
        std::string lower = d.details + " " + d.reasoning;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower.find("skill") != std::string::npos ||
            lower.find("practice") != std::string::npos ||
            lower.find("train") != std::string::npos) {
            return ActionType::PracticeSkill;
        }
    }

    // Rule 5: Details mention social → Socialize
    {
        std::string lower = d.details + " " + d.reasoning;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower.find("social") != std::string::npos ||
            lower.find("collaborate") != std::string::npos ||
            lower.find("team") != std::string::npos) {
            return ActionType::Socialize;
        }
    }

    // Rule 6: Default
    return ActionType::ExecuteTask;
}

} // namespace Systems
