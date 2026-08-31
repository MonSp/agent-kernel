#pragma once

#include "Schema.h"
#include "components/IdentityComponent.h"
#include "components/StatsComponent.h"
#include "components/PersonalityComponent.h"
#include "components/LifecycleComponent.h"
#include "components/SocialComponent.h"
#include "components/MemoryRingComponent.h"
#include "components/SkillTreeComponent.h"
#include "components/CareerComponent.h"
#include "components/EvolutionComponent.h"
#include "ComponentSchemas.h"
#include <vector>
#include <cstring>
#include <unordered_map>
#include <string>
#include <sstream>
#include <algorithm>

namespace ECS {

// ─── GenericComponentStore ────────────────────────────────────────────────────
// Type-erased component storage that stores component instances as raw byte
// buffers, indexed by slot (same slot-based model as the existing Registry).

class GenericComponentStore {
public:
    GenericComponentStore(const ComponentSchema* schema, size_t componentSize)
        : schema_(schema), componentSize_(componentSize) {}
    
    // Check if slot has a component
    bool has(size_t slot) const {
        return slot < presence_.size() && presence_[slot];
    }
    
    // Get raw pointer to component at slot (nullptr if absent)
    void* get(size_t slot) {
        if (!has(slot)) return nullptr;
        return data_.data() + slot * componentSize_;
    }
    const void* get(size_t slot) const {
        if (!has(slot)) return nullptr;
        return data_.data() + slot * componentSize_;
    }
    
    // Set component at slot (copy from src)
    void set(size_t slot, const void* src) {
        growTo(slot + 1);
        std::memcpy(data_.data() + slot * componentSize_, src, componentSize_);
        presence_[slot] = true;
    }
    
    // Remove component at slot (zero the memory)
    void remove(size_t slot) {
        if (!has(slot)) return;
        std::memset(data_.data() + slot * componentSize_, 0, componentSize_);
        presence_[slot] = false;
    }
    
    // Serialize component at slot to JSON using schema
    std::string toJson(size_t slot) const {
        if (!has(slot) || !schema_) return "{}";
        return schema_->instanceToJson(get(slot));
    }
    
    // Ensure capacity for at least n slots
    void growTo(size_t n) {
        if (n <= presence_.size()) return;
        data_.resize(n * componentSize_, 0);
        presence_.resize(n, false);
    }
    
    // Number of slots
    size_t slotCount() const { return presence_.size(); }
    
    // Schema accessor
    const ComponentSchema* schema() const { return schema_; }
    size_t componentSize() const { return componentSize_; }
    
private:
    const ComponentSchema* schema_;
    size_t componentSize_;
    std::vector<uint8_t> data_;       // raw byte buffer: slot * componentSize bytes
    std::vector<bool> presence_;       // which slots have data
};

// ─── DynamicComponentRegistry ─────────────────────────────────────────────────
// Maps component type names to GenericComponentStore instances.

class DynamicComponentRegistry {
public:
    static DynamicComponentRegistry& instance() {
        static DynamicComponentRegistry inst;
        return inst;
    }
    
    // Register a new component type with its schema and size
    void registerComponent(const std::string& name, const ComponentSchema* schema, size_t componentSize) {
        stores_.emplace(name, GenericComponentStore(schema, componentSize));
    }
    
    // Get the store for a component type
    GenericComponentStore* getStore(const std::string& name) {
        auto it = stores_.find(name);
        if (it == stores_.end()) return nullptr;
        return &it->second;
    }
    const GenericComponentStore* getStore(const std::string& name) const {
        auto it = stores_.find(name);
        if (it == stores_.end()) return nullptr;
        return &it->second;
    }
    
    // Check if a component type is registered
    bool hasComponent(const std::string& name) const {
        return stores_.find(name) != stores_.end();
    }
    
    // List all registered component type names
    std::vector<std::string> getAllComponentNames() const {
        std::vector<std::string> names;
        names.reserve(stores_.size());
        for (auto& kv : stores_) {
            names.push_back(kv.first);
        }
        return names;
    }
    
    // Ensure all stores have capacity for at least n slots
    void growAll(size_t n) {
        for (auto& kv : stores_) {
            kv.second.growTo(n);
        }
    }
    
    // Remove component at slot from all stores
    void removeAll(size_t slot) {
        for (auto& kv : stores_) {
            kv.second.remove(slot);
        }
    }
    
    // Serialize all components at a slot to JSON
    std::string allToJson(size_t slot) const {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (auto& kv : stores_) {
            if (!kv.second.has(slot)) continue;
            if (!first) oss << ",";
            oss << "\"" << ComponentSchema::escapeJsonString(kv.first) << "\":"
                << kv.second.toJson(slot);
            first = false;
        }
        oss << "}";
        return oss.str();
    }
    
private:
    DynamicComponentRegistry() = default;
    std::unordered_map<std::string, GenericComponentStore> stores_;
};

// ─── Bridge: initialize DynamicComponentRegistry from existing schemas ────────

inline void initDynamicRegistryFromSchemas() {
    // Ensure all schemas are registered in the SchemaRegistry
    registerAllSchemas();

    auto& schemaReg = ECS::SchemaRegistry::instance();
    auto& dynReg = ECS::DynamicComponentRegistry::instance();

    // Map of schema name → component size (using sizeof on each component type)
    struct ComponentEntry {
        const char* name;
        size_t size;
    };

    ComponentEntry entries[] = {
        {"IdentityComponent",   sizeof(IdentityComponent)},
        {"StatsComponent",      sizeof(StatsComponent)},
        {"PersonalityComponent", sizeof(PersonalityComponent)},
        {"LifecycleComponent",  sizeof(LifecycleComponent)},
        {"SocialComponent",     sizeof(SocialComponent)},
        {"MemoryRingComponent", sizeof(MemoryRingComponent)},
        {"SkillTreeComponent",  sizeof(SkillTreeComponent)},
        {"CareerComponent",     sizeof(CareerComponent)},
        {"EvolutionComponent",  sizeof(EvolutionComponent)},
    };

    for (auto& e : entries) {
        const ComponentSchema* schema = schemaReg.getSchema(e.name);
        if (schema) {
            dynReg.registerComponent(e.name, schema, e.size);
        }
    }
}

} // namespace ECS
