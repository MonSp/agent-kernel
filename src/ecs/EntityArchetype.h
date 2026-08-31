#pragma once

#include "Schema.h"
#include "GenericComponentStore.h"
#include "Registry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace ECS {

// ─── ComponentTemplate ────────────────────────────────────────────────────────
// Describes one component in an archetype: its name and default field values.

struct ComponentTemplate {
    std::string componentName;           // e.g. "StatsComponent"
    std::unordered_map<std::string, std::string> defaults;  // field name → default value as string
};

// ─── EntityArchetype ─────────────────────────────────────────────────────────
// A named template that defines which components an entity should have, with
// default values.  Use instantiate() to stamp out a new entity in a Registry.

struct EntityArchetype {
    std::string name;                    // e.g. "Engineer", "Warrior", "Alchemist"
    std::string description;
    std::vector<ComponentTemplate> components;  // required components with defaults

    // Create an entity from this archetype
    EntityId instantiate(Registry& registry) const;
};

// ─── ArchetypeRegistry ───────────────────────────────────────────────────────
// Global singleton that stores named archetypes and provides factory methods.

class ArchetypeRegistry {
public:
    static ArchetypeRegistry& instance() {
        static ArchetypeRegistry inst;
        return inst;
    }

    void registerArchetype(EntityArchetype archetype) {
        archetypes_[archetype.name] = std::move(archetype);
    }

    const EntityArchetype* getArchetype(const std::string& name) const {
        auto it = archetypes_.find(name);
        return (it != archetypes_.end()) ? &it->second : nullptr;
    }

    std::vector<std::string> getAllArchetypeNames() const {
        std::vector<std::string> names;
        names.reserve(archetypes_.size());
        for (auto& kv : archetypes_) {
            names.push_back(kv.first);
        }
        return names;
    }

    size_t getArchetypeCount() const {
        return archetypes_.size();
    }

    // Create entity from archetype
    EntityId createFromArchetype(const std::string& name, Registry& registry) const {
        const EntityArchetype* arch = getArchetype(name);
        if (!arch) {
            throw std::runtime_error("createFromArchetype: unknown archetype '" + name + "'");
        }
        return arch->instantiate(registry);
    }

private:
    ArchetypeRegistry() = default;
    std::unordered_map<std::string, EntityArchetype> archetypes_;
};

// ─── Default-value application helper ────────────────────────────────────────
// Parses string default values and writes them into a component instance at the
// offsets described by the ComponentSchema.

inline void applyDefaults(void* componentPtr, const ComponentSchema& schema,
                          const std::unordered_map<std::string, std::string>& defaults) {
    uint8_t* base = static_cast<uint8_t*>(componentPtr);
    for (auto& kv : defaults) {
        const FieldDescriptor* field = schema.getField(kv.first);
        if (!field) continue;
        uint8_t* ptr = base + field->offset;
        const std::string& valueStr = kv.second;

        switch (field->type) {
            case FieldType::Bool: {
                bool val = (valueStr == "true" || valueStr == "1");
                std::memcpy(ptr, &val, sizeof(bool));
                break;
            }
            case FieldType::Int8: {
                int8_t val = static_cast<int8_t>(std::stoi(valueStr));
                std::memcpy(ptr, &val, sizeof(int8_t));
                break;
            }
            case FieldType::Int16: {
                int16_t val = static_cast<int16_t>(std::stoi(valueStr));
                std::memcpy(ptr, &val, sizeof(int16_t));
                break;
            }
            case FieldType::Int32: {
                int32_t val = static_cast<int32_t>(std::stoi(valueStr));
                std::memcpy(ptr, &val, sizeof(int32_t));
                break;
            }
            case FieldType::Int64: {
                int64_t val = static_cast<int64_t>(std::stoll(valueStr));
                std::memcpy(ptr, &val, sizeof(int64_t));
                break;
            }
            case FieldType::Uint8: {
                uint8_t val = static_cast<uint8_t>(std::stoul(valueStr));
                std::memcpy(ptr, &val, sizeof(uint8_t));
                break;
            }
            case FieldType::Uint16: {
                uint16_t val = static_cast<uint16_t>(std::stoul(valueStr));
                std::memcpy(ptr, &val, sizeof(uint16_t));
                break;
            }
            case FieldType::Uint32: {
                uint32_t val = static_cast<uint32_t>(std::stoul(valueStr));
                std::memcpy(ptr, &val, sizeof(uint32_t));
                break;
            }
            case FieldType::Uint64: {
                uint64_t val = static_cast<uint64_t>(std::stoull(valueStr));
                std::memcpy(ptr, &val, sizeof(uint64_t));
                break;
            }
            case FieldType::Float32: {
                float val = std::stof(valueStr);
                std::memcpy(ptr, &val, sizeof(float));
                break;
            }
            case FieldType::Float64: {
                double val = std::stod(valueStr);
                std::memcpy(ptr, &val, sizeof(double));
                break;
            }
            case FieldType::String: {
                std::string* strPtr = reinterpret_cast<std::string*>(ptr);
                *strPtr = valueStr;
                break;
            }
            case FieldType::Enum: {
                // Look up the enum name in the field's enumValues list
                for (auto& ev : field->enumValues) {
                    if (ev.second == valueStr) {
                        int64_t enumVal = ev.first;
                        std::memcpy(ptr, &enumVal, field->size);
                        break;
                    }
                }
                break;
            }
            default:
                // Struct, Array, Map — not supported for defaults
                break;
        }
    }
}

// ─── EntityArchetype::instantiate ────────────────────────────────────────────

inline EntityId EntityArchetype::instantiate(Registry& registry) const {
    Entity entity = registry.createEntity();
    EntityId id = entity.getId();

    auto& schemaReg = SchemaRegistry::instance();

    for (const auto& comp : components) {
        const ComponentSchema* schema = schemaReg.getSchema(comp.componentName);
        if (!schema) continue;

        // ── Hardcoded component dispatch ──────────────────────────────────
        if (comp.componentName == "IdentityComponent") {
            auto& c = registry.addComponent<IdentityComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);
        } else if (comp.componentName == "StatsComponent") {
            auto& c = registry.addComponent<StatsComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);
        } else if (comp.componentName == "PersonalityComponent") {
            auto& c = registry.addComponent<PersonalityComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);
        } else if (comp.componentName == "MemoryRingComponent") {
            registry.addComponent<MemoryRingComponent>(id);
            // No introspectable fields
        } else if (comp.componentName == "LifecycleComponent") {
            auto& c = registry.addComponent<LifecycleComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);
        } else if (comp.componentName == "SocialComponent") {
            auto& c = registry.addComponent<SocialComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);
        } else if (comp.componentName == "SkillTreeComponent") {
            registry.addComponent<SkillTreeComponent>(id);
            // No introspectable fields (unordered_map)
        } else if (comp.componentName == "CareerComponent") {
            auto& c = registry.addComponent<CareerComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);
        } else if (comp.componentName == "EvolutionComponent") {
            auto& c = registry.addComponent<EvolutionComponent>(id);
            applyDefaults(&c, *schema, comp.defaults);

        // ── Dynamic component path ───────────────────────────────────────
        } else {
            auto* store = DynamicComponentRegistry::instance().getStore(comp.componentName);
            if (store) {
                std::vector<uint8_t> buf(store->componentSize(), 0);
                applyDefaults(buf.data(), *schema, comp.defaults);
                registry.setDynamicComponent(id, comp.componentName, buf.data());
            }
        }
    }

    return id;
}

} // namespace ECS
