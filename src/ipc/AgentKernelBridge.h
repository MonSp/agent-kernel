#pragma once

#include "Protocol.h"
#include "UnixSocketServer.h"
#include "../ecs/Registry.h"
#include "../ecs/components/IdentityComponent.h"
#include "../ecs/components/SkillTreeComponent.h"
#include "../ecs/components/CareerComponent.h"
#include <string>
#include <functional>
#include <cstdio>

namespace IPC {

// --- Minimal JSON helpers (no external dependency) ---

namespace json {

// Escape a string for JSON embedding
inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Find value of a top-level string key: "key":"value"
// Returns empty string if not found.
inline std::string getString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) return "";

    size_t quoteStart = json.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) return "";

    size_t quoteEnd = quoteStart + 1;
    while (quoteEnd < json.size()) {
        if (json[quoteEnd] == '\\') {
            quoteEnd += 2; // skip escaped char
            continue;
        }
        if (json[quoteEnd] == '"') break;
        ++quoteEnd;
    }
    if (quoteEnd >= json.size()) return "";

    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

// Find the raw JSON object/string/number value after "key":
// Returns the substring starting at the value token.
inline std::string getRawValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) return "";

    size_t valStart = colonPos + 1;
    while (valStart < json.size() && (json[valStart] == ' ' || json[valStart] == '\t')) ++valStart;

    if (valStart >= json.size()) return "";

    if (json[valStart] == '"') {
        // string value — find closing quote
        size_t end = valStart + 1;
        while (end < json.size()) {
            if (json[end] == '\\') { end += 2; continue; }
            if (json[end] == '"') break;
            ++end;
        }
        return json.substr(valStart, end - valStart + 1);
    } else if (json[valStart] == '{') {
        // object — find matching brace
        int depth = 0;
        size_t end = valStart;
        while (end < json.size()) {
            if (json[end] == '{') ++depth;
            else if (json[end] == '}') { --depth; if (depth == 0) break; }
            ++end;
        }
        return json.substr(valStart, end - valStart + 1);
    } else if (json[valStart] == '[') {
        // array — find matching bracket
        int depth = 0;
        size_t end = valStart;
        while (end < json.size()) {
            if (json[end] == '[') ++depth;
            else if (json[end] == ']') { --depth; if (depth == 0) break; }
            ++end;
        }
        return json.substr(valStart, end - valStart + 1);
    } else {
        // number/bool/null — read until comma or brace
        size_t end = valStart;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']'
               && json[end] != ' ' && json[end] != '\n') ++end;
        return json.substr(valStart, end - valStart);
    }
}

// Get integer value
inline int getInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::string raw = getRawValue(json, key);
    if (raw.empty()) return defaultVal;
    try { return std::stoi(raw); } catch (...) { return defaultVal; }
}

// Get unsigned integer value
inline uint32_t getUint(const std::string& json, const std::string& key, uint32_t defaultVal = 0) {
    std::string raw = getRawValue(json, key);
    if (raw.empty()) return defaultVal;
    try { return static_cast<uint32_t>(std::stoul(raw)); } catch (...) { return defaultVal; }
}

inline std::string ok(const std::string& dataJson) {
    return "{\"ok\":true,\"data\":" + dataJson + "}";
}

inline std::string error(const std::string& msg) {
    return "{\"ok\":false,\"error\":\"" + escape(msg) + "\"}";
}

} // namespace json

// --- Enum string conversions ---

inline std::string roleToString(AgentRole r) {
    switch (r) {
        case AgentRole::Worker:     return "Worker";
        case AgentRole::Specialist: return "Specialist";
        case AgentRole::Lead:       return "Lead";
        case AgentRole::Manager:    return "Manager";
        case AgentRole::Director:   return "Director";
        default: return "Worker";
    }
}

inline AgentRole stringToRole(const std::string& s) {
    if (s == "Specialist") return AgentRole::Specialist;
    if (s == "Lead")       return AgentRole::Lead;
    if (s == "Manager")    return AgentRole::Manager;
    if (s == "Director")   return AgentRole::Director;
    return AgentRole::Worker;
}

inline std::string skillLevelToString(SkillLevel l) {
    switch (l) {
        case SkillLevel::None:         return "None";
        case SkillLevel::Beginner:     return "Beginner";
        case SkillLevel::Intermediate: return "Intermediate";
        case SkillLevel::Advanced:     return "Advanced";
        case SkillLevel::Expert:       return "Expert";
        default: return "None";
    }
}

inline std::string skillCategoryToString(SkillCategory c) {
    switch (c) {
        case SkillCategory::Engineering: return "Engineering";
        case SkillCategory::Design:      return "Design";
        case SkillCategory::Content:     return "Content";
        case SkillCategory::Data:        return "Data";
        case SkillCategory::Management:  return "Management";
        default: return "Engineering";
    }
}

inline SkillCategory stringToSkillCategory(const std::string& s) {
    if (s == "Design")      return SkillCategory::Design;
    if (s == "Content")     return SkillCategory::Content;
    if (s == "Data")        return SkillCategory::Data;
    if (s == "Management")  return SkillCategory::Management;
    return SkillCategory::Engineering;
}

inline std::string careerStageToString(CareerStage s) {
    switch (s) {
        case CareerStage::Junior:  return "Junior";
        case CareerStage::Mid:     return "Mid";
        case CareerStage::Senior:  return "Senior";
        case CareerStage::Lead:    return "Lead";
        case CareerStage::Expert:  return "Expert";
        default: return "Junior";
    }
}

// --- Component serializers ---

inline std::string toJson(const IdentityComponent& id) {
    return "{\"id\":\"" + json::escape(id.id) +
           "\",\"name\":\"" + json::escape(id.name) +
           "\",\"department\":\"" + json::escape(id.department) +
           "\",\"companyRole\":\"" + json::escape(id.companyRole) +
           "\",\"teamId\":\"" + json::escape(id.teamId) +
           "\",\"role\":\"" + roleToString(id.role) + "\"}";
}

inline std::string toJson(const SkillNode& node) {
    std::string deps = "[";
    for (size_t i = 0; i < node.dependencies.size(); ++i) {
        if (i > 0) deps += ",";
        deps += "\"" + json::escape(node.dependencies[i]) + "\"";
    }
    deps += "]";

    return "{\"skillId\":\"" + json::escape(node.skillId) +
           "\",\"category\":\"" + skillCategoryToString(node.category) +
           "\",\"level\":\"" + skillLevelToString(node.level) +
           "\",\"xp\":" + std::to_string(node.xp) +
           ",\"usageCount\":" + std::to_string(node.usageCount) +
           ",\"successCount\":" + std::to_string(node.successCount) +
           ",\"effectiveness\":" + std::to_string(node.effectiveness) +
           ",\"dependencies\":" + deps + "}";
}

inline std::string toJson(const SkillTreeComponent& tree) {
    std::string result = "{";
    bool first = true;
    for (const auto& [key, node] : tree.skills) {
        if (!first) result += ",";
        first = false;
        result += "\"" + json::escape(key) + "\":" + toJson(node);
    }
    result += "}";
    return result;
}

inline std::string toJson(const CareerComponent& career) {
    return "{\"totalXp\":" + std::to_string(career.totalXp) +
           ",\"stage\":\"" + careerStageToString(career.stage) +
           "\",\"tasksCompleted\":" + std::to_string(career.tasksCompleted) +
           ",\"tasksSucceeded\":" + std::to_string(career.tasksSucceeded) +
           ",\"avgReviewScore\":" + std::to_string(career.avgReviewScore) + "}";
}

// Serialize a full agent (all relevant components)
inline std::string agentToJson(ECS::EntityId entityId) {
    auto& reg = ECS::Registry::getInstance();
    std::string result = "{\"entityId\":" + std::to_string(entityId);

    auto* identity = reg.getComponent<IdentityComponent>(entityId);
    if (identity) {
        result += ",\"identity\":" + toJson(*identity);
    }

    auto* skillTree = reg.getComponent<SkillTreeComponent>(entityId);
    if (skillTree) {
        result += ",\"skillTree\":" + toJson(*skillTree);
    }

    auto* career = reg.getComponent<CareerComponent>(entityId);
    if (career) {
        result += ",\"career\":" + toJson(*career);
    }

    result += "}";
    return result;
}

// --- Bridge class ---

class AgentKernelBridge {
public:
    explicit AgentKernelBridge(const std::string& socketPath)
        : server_(socketPath) {
        server_.setRequestHandler([this](const std::string& raw) {
            return handleRequest(raw);
        });
    }

    bool start() { return server_.start(); }
    void stop()  { server_.stop(); }
    bool isRunning() const { return server_.isRunning(); }

private:
    std::string handleRequest(const std::string& raw) {
        std::string method = json::getString(raw, "method");
        if (method.empty()) {
            return json::error("missing method");
        }

        if (method == Method::createAgent)  return handleCreateAgent(raw);
        if (method == Method::getAgent)     return handleGetAgent(raw);
        if (method == Method::updateAgent)  return handleUpdateAgent(raw);
        if (method == Method::deleteAgent)  return handleDeleteAgent(raw);
        if (method == Method::listAgents)   return handleListAgents(raw);
        if (method == Method::addSkill)     return handleAddSkill(raw);
        if (method == Method::addSkillXp)   return handleAddSkillXp(raw);
        if (method == Method::addCareerXp)  return handleAddCareerXp(raw);
        if (method == Method::getSkills)    return handleGetSkills(raw);
        if (method == Method::syncState)    return handleSyncState(raw);

        return json::error("unknown method: " + method);
    }

    // createAgent: { "method":"createAgent", "params": { "id":"...", "name":"...", "department":"...", "companyRole":"...", "teamId":"...", "role":"Worker" } }
    std::string handleCreateAgent(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        std::string agentId   = json::getString(params, "id");
        std::string name      = json::getString(params, "name");
        std::string dept      = json::getString(params, "department");
        std::string crole     = json::getString(params, "companyRole");
        std::string teamId    = json::getString(params, "teamId");
        std::string roleStr   = json::getString(params, "role");

        if (agentId.empty() || name.empty()) {
            return json::error("id and name are required");
        }

        auto& reg = ECS::Registry::getInstance();
        ECS::Entity entity = reg.createEntity();
        ECS::EntityId eid = entity.getId();

        AgentRole role = stringToRole(roleStr);
        reg.addComponent<IdentityComponent>(eid, agentId, name, dept, crole, teamId, role);
        reg.addComponent<SkillTreeComponent>(eid);
        reg.addComponent<CareerComponent>(eid);

        return json::ok(agentToJson(eid));
    }

    // getAgent: { "method":"getAgent", "params": { "entityId": 0 } }
    std::string handleGetAgent(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        return json::ok(agentToJson(entityId));
    }

    // updateAgent: { "method":"updateAgent", "params": { "entityId": 0, "name":"...", ... } }
    std::string handleUpdateAgent(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        auto* identity = reg.getComponent<IdentityComponent>(entityId);
        if (identity) {
            std::string val;
            val = json::getString(params, "name");
            if (!val.empty()) identity->name = val;
            val = json::getString(params, "department");
            if (!val.empty()) identity->department = val;
            val = json::getString(params, "companyRole");
            if (!val.empty()) identity->companyRole = val;
            val = json::getString(params, "teamId");
            if (!val.empty()) identity->teamId = val;
            val = json::getString(params, "role");
            if (!val.empty()) identity->role = stringToRole(val);
        }

        return json::ok(agentToJson(entityId));
    }

    // deleteAgent: { "method":"deleteAgent", "params": { "entityId": 0 } }
    std::string handleDeleteAgent(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        reg.destroyEntity(entityId);
        return json::ok("{\"deleted\":true}");
    }

    // listAgents: { "method":"listAgents" }
    std::string handleListAgents(const std::string& /*raw*/) {
        auto& reg = ECS::Registry::getInstance();
        auto entities = reg.getAllEntities();

        std::string result = "[";
        for (size_t i = 0; i < entities.size(); ++i) {
            if (i > 0) result += ",";
            result += agentToJson(entities[i]);
        }
        result += "]";
        return json::ok(result);
    }

    // addSkill: { "method":"addSkill", "params": { "entityId": 0, "skillId":"...", "category":"Engineering" } }
    std::string handleAddSkill(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));
        std::string skillId = json::getString(params, "skillId");
        std::string catStr = json::getString(params, "category");

        if (skillId.empty()) return json::error("skillId is required");

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        auto* tree = reg.getComponent<SkillTreeComponent>(entityId);
        if (!tree) {
            return json::error("entity has no skill tree");
        }

        SkillCategory cat = stringToSkillCategory(catStr);
        tree->addSkill(skillId, cat);

        SkillNode* node = tree->getSkill(skillId);
        return json::ok(toJson(*node));
    }

    // addSkillXp: { "method":"addSkillXp", "params": { "entityId": 0, "skillId":"...", "xp": 100 } }
    std::string handleAddSkillXp(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));
        std::string skillId = json::getString(params, "skillId");
        uint32_t xp = json::getUint(params, "xp", 0);

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        auto* tree = reg.getComponent<SkillTreeComponent>(entityId);
        if (!tree) {
            return json::error("entity has no skill tree");
        }

        if (!tree->hasSkill(skillId)) {
            return json::error("skill not found: " + skillId);
        }

        tree->addXp(skillId, xp);

        SkillNode* node = tree->getSkill(skillId);
        return json::ok(toJson(*node));
    }

    // addCareerXp: { "method":"addCareerXp", "params": { "entityId": 0, "xp": 600 } }
    std::string handleAddCareerXp(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));
        uint32_t xp = json::getUint(params, "xp", 0);

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        auto* career = reg.getComponent<CareerComponent>(entityId);
        if (!career) {
            return json::error("entity has no career component");
        }

        career->addXp(xp);
        return json::ok(toJson(*career));
    }

    // getSkills: { "method":"getSkills", "params": { "entityId": 0 } }
    std::string handleGetSkills(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        auto* tree = reg.getComponent<SkillTreeComponent>(entityId);
        if (!tree) {
            return json::error("entity has no skill tree");
        }

        return json::ok(toJson(*tree));
    }

    // syncState: { "method":"syncState" }
    std::string handleSyncState(const std::string& /*raw*/) {
        auto& reg = ECS::Registry::getInstance();
        auto entities = reg.getAllEntities();

        std::string result = "{\"agents\":[";
        for (size_t i = 0; i < entities.size(); ++i) {
            if (i > 0) result += ",";
            result += agentToJson(entities[i]);
        }
        result += "],\"count\":" + std::to_string(entities.size()) + "}";
        return json::ok(result);
    }

    UnixSocketServer server_;
};

} // namespace IPC
