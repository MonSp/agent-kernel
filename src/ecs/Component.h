#pragma once

#include <cstdint>
#include <typeindex>
#include <typeinfo>
#include <memory>
#include <vector>
#include <unordered_map>
#include <set>
#include <atomic>

namespace ECS {

using EntityId = uint64_t;
using ComponentTypeId = uint32_t;

inline ComponentTypeId generateComponentTypeId() {
    static std::atomic<ComponentTypeId> counter{0};
    return counter.fetch_add(1);
}

class IComponent {
public:
    virtual ~IComponent() = default;
    virtual ComponentTypeId getTypeId() const = 0;
};

template<typename T>
class ComponentBase : public IComponent {
public:
    static ComponentTypeId getStaticTypeId() {
        static ComponentTypeId id = generateComponentTypeId();
        return id;
    }

    ComponentTypeId getTypeId() const override {
        return T::getStaticTypeId();
    }
};

template<typename T>
class Component : public ComponentBase<T> {
public:
    T value;

    Component() : value() {}
    Component(const T& val) : value(val) {}
    Component(T&& val) : value(std::move(val)) {}

    T& get() { return value; }
    const T& get() const { return value; }
};

struct ComponentTypeInfo {
    std::type_index type;
    size_t size;
    size_t alignment;

    ComponentTypeInfo() : type(typeid(void)), size(0), alignment(0) {}
    ComponentTypeInfo(std::type_index t, size_t s, size_t a) : type(t), size(s), alignment(a) {}
};

class ComponentRegistry {
public:
    static ComponentRegistry& getInstance() {
        static ComponentRegistry instance;
        return instance;
    }

    template<typename T>
    void registerComponent() {
        ComponentTypeId id = ComponentBase<T>::getStaticTypeId();
        auto it = componentInfos.find(id);
        if (it == componentInfos.end()) {
            componentInfos.emplace(id, ComponentTypeInfo{std::type_index(typeid(T)), sizeof(T), alignof(T)});
        }
    }

    ComponentTypeInfo getComponentInfo(ComponentTypeId id) const {
        static ComponentTypeInfo empty;
        auto it = componentInfos.find(id);
        return it != componentInfos.end() ? it->second : empty;
    }

    size_t getComponentCount() const {
        return componentInfos.size();
    }

private:
    ComponentRegistry() = default;
    std::unordered_map<ComponentTypeId, ComponentTypeInfo> componentInfos;
};

}
