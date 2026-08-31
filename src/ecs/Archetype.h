#pragma once

#include "Component.h"
#include "Entity.h"
#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>

namespace ECS {

struct Archetype {
    std::vector<ComponentTypeId> componentTypes;
    std::vector<EntityId> entities;
};

class ArchetypeManager {
public:
    static ArchetypeManager& getInstance() {
        static ArchetypeManager instance;
        return instance;
    }

    Archetype* getOrCreateArchetype(const std::vector<ComponentTypeId>& types) {
        std::vector<ComponentTypeId> sortedTypes = types;
        std::sort(sortedTypes.begin(), sortedTypes.end());

        std::string key = typesToKey(sortedTypes);
        auto it = archetypes.find(key);
        if (it != archetypes.end()) {
            return &it->second;
        }

        Archetype arch;
        arch.componentTypes = sortedTypes;
        archetypes[key] = arch;
        return &archetypes[key];
    }

    void addEntityToArchetype(Archetype* arch, EntityId id) {
        arch->entities.push_back(id);
    }

    void removeEntityFromArchetype(Archetype* arch, EntityId id) {
        auto it = std::find(arch->entities.begin(), arch->entities.end(), id);
        if (it != arch->entities.end()) {
            arch->entities.erase(it);
        }
    }

    std::vector<Archetype*> getAllArchetypes() {
        std::vector<Archetype*> result;
        for (auto& pair : archetypes) {
            result.push_back(&pair.second);
        }
        return result;
    }

    size_t getArchetypeCount() const {
        return archetypes.size();
    }

private:
    ArchetypeManager() = default;

    std::string typesToKey(const std::vector<ComponentTypeId>& types) {
        std::string key;
        for (auto type : types) {
            key += std::to_string(type) + ",";
        }
        return key;
    }

    std::unordered_map<std::string, Archetype> archetypes;
};

}
