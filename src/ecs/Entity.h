#pragma once

#include "Component.h"
#include <vector>
#include <algorithm>

namespace ECS {

class Entity {
public:
    EntityId getId() const { return id; }
    bool isValid() const { return valid; }

private:
    EntityId id;
    bool valid;

    Entity(EntityId id) : id(id), valid(true) {}
    void invalidate() { valid = false; }

    friend class Registry;
};

}
