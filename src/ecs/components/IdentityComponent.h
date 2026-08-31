#pragma once

#include "../Component.h"
#include <string>

enum class AgentRole : uint8_t {
    Worker = 0,
    Specialist = 1,
    Lead = 2,
    Manager = 3,
    Director = 4
};

struct IdentityComponent : public ECS::ComponentBase<IdentityComponent> {
    std::string id;
    std::string name;
    std::string department;
    std::string companyRole;
    std::string teamId;
    AgentRole role;

    IdentityComponent() : role(AgentRole::Worker) {}

    IdentityComponent(const std::string& nid, const std::string& nname,
                      AgentRole r)
        : id(nid), name(nname), role(r) {}

    IdentityComponent(const std::string& nid, const std::string& nname,
                      const std::string& dept, const std::string& crole,
                      const std::string& tid, AgentRole r)
        : id(nid), name(nname), department(dept), companyRole(crole),
          teamId(tid), role(r) {}

    bool isLeader() const {
        return role == AgentRole::Lead ||
               role == AgentRole::Manager ||
               role == AgentRole::Director;
    }
};
