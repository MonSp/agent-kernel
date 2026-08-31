#pragma once

#include "../Component.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

enum class SkillLevel : uint8_t {
    None = 0,
    Beginner = 1,
    Intermediate = 2,
    Advanced = 3,
    Expert = 4
};

enum class SkillCategory : uint8_t {
    Engineering = 0,
    Design = 1,
    Content = 2,
    Data = 3,
    Management = 4
};

struct SkillNode {
    std::string skillId;
    SkillCategory category;
    SkillLevel level;
    uint32_t xp;
    uint32_t usageCount;
    uint32_t successCount;
    float effectiveness;  // 0.0-1.0
    std::vector<std::string> dependencies;

    SkillNode()
        : category(SkillCategory::Engineering), level(SkillLevel::None),
          xp(0), usageCount(0), successCount(0), effectiveness(0.0f) {}

    SkillNode(const std::string& id, SkillCategory cat, SkillLevel lvl = SkillLevel::Beginner)
        : skillId(id), category(cat), level(lvl),
          xp(0), usageCount(0), successCount(0), effectiveness(0.0f) {}
};

struct SkillTreeComponent : public ECS::ComponentBase<SkillTreeComponent> {
    std::unordered_map<std::string, SkillNode> skills;

    SkillTreeComponent() = default;

    void addSkill(const std::string& skillId, SkillCategory category,
                  SkillLevel level = SkillLevel::Beginner,
                  const std::vector<std::string>& deps = {}) {
        SkillNode node(skillId, category, level);
        node.dependencies = deps;
        skills[skillId] = node;
    }

    SkillNode* getSkill(const std::string& skillId) {
        auto it = skills.find(skillId);
        if (it == skills.end()) return nullptr;
        return &it->second;
    }

    const SkillNode* getSkill(const std::string& skillId) const {
        auto it = skills.find(skillId);
        if (it == skills.end()) return nullptr;
        return &it->second;
    }

    bool hasSkill(const std::string& skillId) const {
        return skills.find(skillId) != skills.end();
    }

    // Add XP to a skill; auto level-up when xp >= 1000
    bool addXp(const std::string& skillId, uint32_t amount) {
        auto it = skills.find(skillId);
        if (it == skills.end()) return false;

        it->second.xp += amount;

        // Auto level-up: every 1000 XP promotes one level
        while (it->second.xp >= 1000 &&
               it->second.level != SkillLevel::Expert) {
            it->second.xp -= 1000;
            it->second.level = static_cast<SkillLevel>(
                static_cast<uint8_t>(it->second.level) + 1);
        }
        return true;
    }

    // Returns average level across all skills (0.0-4.0)
    float getOverallLevel() const {
        if (skills.empty()) return 0.0f;
        float total = 0.0f;
        for (const auto& [id, node] : skills) {
            total += static_cast<float>(static_cast<uint8_t>(node.level));
        }
        return total / static_cast<float>(skills.size());
    }
};
