#pragma once

#include "Component.h"
#include "Entity.h"
#include "components/IdentityComponent.h"
#include "components/StatsComponent.h"
#include "components/PersonalityComponent.h"
#include "components/MemoryRingComponent.h"
#include "components/LifecycleComponent.h"
#include "components/SocialComponent.h"
#include "components/SkillTreeComponent.h"
#include "components/CareerComponent.h"
#include "components/EvolutionComponent.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include <optional>
#include <stdexcept>

namespace ECS {

class Registry {
public:
    static Registry& getInstance() {
        static Registry instance;
        return instance;
    }

    Entity createEntity() {
        size_t slot;
        if (freeSlots_.empty()) {
            slot = entityIds_.size();
            entityIds_.push_back(0);
            activeSlots_.push_back(false);
            growComponentArrays(slot + 1);
        } else {
            slot = freeSlots_.front();
            freeSlots_.pop();
        }

        EntityId id;
        if (freeIds_.empty()) {
            id = nextEntityId_++;
        } else {
            id = freeIds_.front();
            freeIds_.pop();
        }

        entityIds_[slot] = id;
        entityToSlot_[id] = slot;
        activeSlots_[slot] = true;
        return Entity(id);
    }

    void destroyEntity(EntityId id) {
        auto it = entityToSlot_.find(id);
        if (it == entityToSlot_.end()) return;
        size_t slot = it->second;
        if (!activeSlots_[slot]) return;

        activeSlots_[slot] = false;
        identityComponents_[slot].reset();
        statsComponents_[slot].reset();
        personalityComponents_[slot].reset();
        memoryComponents_[slot].reset();
        lifecycleComponents_[slot].reset();
        socialComponents_[slot].reset();
        skillTreeComponents_[slot].reset();
        careerComponents_[slot].reset();
        evolutionComponents_[slot].reset();

        freeSlots_.push(slot);
        freeIds_.push(id);
        entityToSlot_.erase(it);
    }

    bool isEntityValid(EntityId id) const {
        auto it = entityToSlot_.find(id);
        if (it == entityToSlot_.end()) return false;
        return activeSlots_[it->second];
    }

    std::vector<EntityId> getAllEntities() const {
        std::vector<EntityId> result;
        result.reserve(entityIds_.size());
        for (size_t i = 0; i < entityIds_.size(); ++i) {
            if (activeSlots_[i]) {
                result.push_back(entityIds_[i]);
            }
        }
        return result;
    }

    size_t getEntityCount() const {
        return entityToSlot_.size();
    }

    void clear() {
        entityIds_.clear();
        activeSlots_.clear();
        entityToSlot_.clear();
        while (!freeSlots_.empty()) freeSlots_.pop();
        while (!freeIds_.empty()) freeIds_.pop();
        nextEntityId_ = 0;

        identityComponents_.clear();
        statsComponents_.clear();
        personalityComponents_.clear();
        memoryComponents_.clear();
        lifecycleComponents_.clear();
        socialComponents_.clear();
        skillTreeComponents_.clear();
        careerComponents_.clear();
        evolutionComponents_.clear();
    }

    // --- Component CRUD via if constexpr dispatch ---

    template<typename T>
    std::vector<std::optional<T>>& getArray();

    template<typename T>
    T* getComponent(EntityId id) {
        auto it = entityToSlot_.find(id);
        if (it == entityToSlot_.end() || !activeSlots_[it->second]) return nullptr;
        auto& arr = getArray<T>();
        if (it->second >= arr.size()) return nullptr;
        if (!arr[it->second].has_value()) return nullptr;
        return &arr[it->second].value();
    }

    template<typename T, typename... Args>
    T& addComponent(EntityId id, Args&&... args) {
        auto it = entityToSlot_.find(id);
        if (it == entityToSlot_.end() || !activeSlots_[it->second]) {
            throw std::runtime_error("addComponent: invalid entity");
        }
        size_t slot = it->second;
        auto& arr = getArray<T>();
        if (slot >= arr.size()) {
            arr.resize(slot + 1);
        }
        arr[slot].emplace(std::forward<Args>(args)...);
        return arr[slot].value();
    }

    template<typename T>
    bool hasComponent(EntityId id) const {
        auto it = entityToSlot_.find(id);
        if (it == entityToSlot_.end() || !activeSlots_[it->second]) return false;
        return hasComponentAtSlot<T>(it->second);
    }

    template<typename T>
    std::vector<EntityId> getEntitiesWithComponent() {
        std::vector<EntityId> result;
        auto& arr = getArray<T>();
        for (size_t i = 0; i < entityIds_.size(); ++i) {
            if (activeSlots_[i] && i < arr.size() && arr[i].has_value()) {
                result.push_back(entityIds_[i]);
            }
        }
        return result;
    }

private:
    Registry() = default;

    std::vector<EntityId> entityIds_;
    std::vector<bool> activeSlots_;
    std::unordered_map<EntityId, size_t> entityToSlot_;

    EntityId nextEntityId_ = 0;
    std::queue<EntityId> freeIds_;
    std::queue<size_t> freeSlots_;

    // Component arrays (optional-per-slot)
    std::vector<std::optional<IdentityComponent>> identityComponents_;
    std::vector<std::optional<StatsComponent>> statsComponents_;
    std::vector<std::optional<PersonalityComponent>> personalityComponents_;
    std::vector<std::optional<MemoryRingComponent>> memoryComponents_;
    std::vector<std::optional<LifecycleComponent>> lifecycleComponents_;
    std::vector<std::optional<SocialComponent>> socialComponents_;
    std::vector<std::optional<SkillTreeComponent>> skillTreeComponents_;
    std::vector<std::optional<CareerComponent>> careerComponents_;
    std::vector<std::optional<EvolutionComponent>> evolutionComponents_;

    void growComponentArrays(size_t newSize) {
        if (identityComponents_.size() < newSize) identityComponents_.resize(newSize);
        if (statsComponents_.size() < newSize) statsComponents_.resize(newSize);
        if (personalityComponents_.size() < newSize) personalityComponents_.resize(newSize);
        if (memoryComponents_.size() < newSize) memoryComponents_.resize(newSize);
        if (lifecycleComponents_.size() < newSize) lifecycleComponents_.resize(newSize);
        if (socialComponents_.size() < newSize) socialComponents_.resize(newSize);
        if (skillTreeComponents_.size() < newSize) skillTreeComponents_.resize(newSize);
        if (careerComponents_.size() < newSize) careerComponents_.resize(newSize);
        if (evolutionComponents_.size() < newSize) evolutionComponents_.resize(newSize);
    }

    template<typename T>
    bool hasComponentAtSlot(size_t slot) const {
        if constexpr (std::is_same_v<T, IdentityComponent>) {
            return slot < identityComponents_.size() && identityComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, StatsComponent>) {
            return slot < statsComponents_.size() && statsComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, PersonalityComponent>) {
            return slot < personalityComponents_.size() && personalityComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, MemoryRingComponent>) {
            return slot < memoryComponents_.size() && memoryComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, LifecycleComponent>) {
            return slot < lifecycleComponents_.size() && lifecycleComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, SocialComponent>) {
            return slot < socialComponents_.size() && socialComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, SkillTreeComponent>) {
            return slot < skillTreeComponents_.size() && skillTreeComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, CareerComponent>) {
            return slot < careerComponents_.size() && careerComponents_[slot].has_value();
        } else if constexpr (std::is_same_v<T, EvolutionComponent>) {
            return slot < evolutionComponents_.size() && evolutionComponents_[slot].has_value();
        }
        return false;
    }
};

// Template specializations for getArray
template<>
inline std::vector<std::optional<IdentityComponent>>& Registry::getArray<IdentityComponent>() {
    return identityComponents_;
}
template<>
inline std::vector<std::optional<StatsComponent>>& Registry::getArray<StatsComponent>() {
    return statsComponents_;
}
template<>
inline std::vector<std::optional<PersonalityComponent>>& Registry::getArray<PersonalityComponent>() {
    return personalityComponents_;
}
template<>
inline std::vector<std::optional<MemoryRingComponent>>& Registry::getArray<MemoryRingComponent>() {
    return memoryComponents_;
}
template<>
inline std::vector<std::optional<LifecycleComponent>>& Registry::getArray<LifecycleComponent>() {
    return lifecycleComponents_;
}
template<>
inline std::vector<std::optional<SocialComponent>>& Registry::getArray<SocialComponent>() {
    return socialComponents_;
}
template<>
inline std::vector<std::optional<SkillTreeComponent>>& Registry::getArray<SkillTreeComponent>() {
    return skillTreeComponents_;
}
template<>
inline std::vector<std::optional<CareerComponent>>& Registry::getArray<CareerComponent>() {
    return careerComponents_;
}
template<>
inline std::vector<std::optional<EvolutionComponent>>& Registry::getArray<EvolutionComponent>() {
    return evolutionComponents_;
}

} // namespace ECS
