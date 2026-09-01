#pragma once

#include "Protocol.h"
#include "UnixSocketServer.h"
#include "../ecs/Registry.h"
#include "../ecs/Schema.h"
#include "../ecs/SchemaValidator.h"
#include "../ecs/ComponentSchemas.h"
#include "../ecs/EntityArchetype.h"
#include "../ecs/BuiltinArchetypes.h"
#include "../ecs/components/IdentityComponent.h"
#include "../ecs/components/StatsComponent.h"
#include "../ecs/components/PersonalityComponent.h"
#include "../ecs/components/LifecycleComponent.h"
#include "../ecs/components/SocialComponent.h"
#include "../ecs/components/MemoryRingComponent.h"
#include "../ecs/components/SkillTreeComponent.h"
#include "../ecs/components/CareerComponent.h"
#include "../ecs/components/EvolutionComponent.h"
#include "../llm/DecisionEngine.h"
#include "../ecs/systems/TickEngine.h"
#include "../ecs/systems/SimulationRunner.h"
#include <string>
#include <functional>
#include <cstdio>
#include <vector>

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

// Compact JSON by stripping embedded newlines/carriage returns
// (needed when schema output contains \n that conflicts with newline-delimited IPC)
inline std::string compact(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c != '\n' && c != '\r') out += c;
    }
    return out;
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

inline std::string realmLevelToString(RealmLevel r) {
    switch (r) {
        case RealmLevel::Mortal:            return "Mortal";
        case RealmLevel::QiRefining:        return "QiRefining";
        case RealmLevel::FoundationBuilding: return "FoundationBuilding";
        case RealmLevel::GoldenCore:        return "GoldenCore";
        case RealmLevel::YuanInfant:        return "YuanInfant";
        case RealmLevel::Transcension:      return "Transcension";
        default: return "Mortal";
    }
}

inline std::string toJson(const StatsComponent& stats) {
    return "{\"power\":" + std::to_string(stats.power) +
           ",\"hp\":" + std::to_string(stats.hp) +
           ",\"maxHp\":" + std::to_string(stats.maxHp) +
           ",\"mp\":" + std::to_string(stats.mp) +
           ",\"maxMp\":" + std::to_string(stats.maxMp) +
           ",\"realm\":\"" + realmLevelToString(stats.realm) +
           "\",\"xp\":" + std::to_string(stats.xp) +
           ",\"careerLevel\":" + std::to_string(stats.careerLevel) + "}";
}

inline std::string toJson(const PersonalityComponent& p) {
    return "{\"ambition\":" + std::to_string(p.ambition) +
           ",\"caution\":" + std::to_string(p.caution) +
           ",\"loyalty\":" + std::to_string(p.loyalty) +
           ",\"greed\":" + std::to_string(p.greed) +
           ",\"sociability\":" + std::to_string(p.sociability) +
           ",\"diligence\":" + std::to_string(p.diligence) + "}";
}

inline std::string lifeStateToString(AgentLifeState s) {
    switch (s) {
        case AgentLifeState::Idle:       return "Idle";
        case AgentLifeState::Active:     return "Active";
        case AgentLifeState::Paused:     return "Paused";
        case AgentLifeState::Terminated: return "Terminated";
        default: return "Idle";
    }
}

inline std::string birthTypeToString(BirthType b) {
    switch (b) {
        case BirthType::Natural:    return "Natural";
        case BirthType::WarOrphan:  return "WarOrphan";
        case BirthType::Wanderer:   return "Wanderer";
        case BirthType::DemonBeast: return "DemonBeast";
        default: return "Natural";
    }
}

inline std::string toJson(const LifecycleComponent& lc) {
    return "{\"birthTime\":" + std::to_string(lc.birthTime) +
           ",\"age\":" + std::to_string(lc.age) +
           ",\"lifeState\":\"" + lifeStateToString(lc.lifeState) +
           "\",\"birthType\":\"" + birthTypeToString(lc.birthType) +
           "\",\"lastUpdateTime\":" + std::to_string(lc.lastUpdateTime) + "}";
}

inline std::string toJson(const SocialComponent& sc) {
    return "{\"hunger\":" + std::to_string(sc.hunger) +
           ",\"fatigue\":" + std::to_string(sc.fatigue) +
           ",\"energy\":" + std::to_string(sc.energy) +
           ",\"socialDesire\":" + std::to_string(sc.socialDesire) +
           ",\"mood\":" + std::to_string(sc.mood) +
           ",\"anger\":" + std::to_string(sc.anger) +
           ",\"fear\":" + std::to_string(sc.fear) +
           ",\"joy\":" + std::to_string(sc.joy) + "}";
}

inline std::string toJson(const EvolutionComponent& evo) {
    return "{\"totalEvolutions\":" + std::to_string(evo.totalEvolutions) +
           ",\"successfulEvolutions\":" + std::to_string(evo.successfulEvolutions) +
           ",\"diversityScore\":" + std::to_string(evo.diversityScore) +
           ",\"historySize\":" + std::to_string(evo.history.size()) + "}";
}

// Serialize a full agent (all 9 components)
inline std::string agentToJson(ECS::EntityId entityId) {
    auto& reg = ECS::Registry::getInstance();
    std::string result = "{\"entityId\":" + std::to_string(entityId);

    auto* identity = reg.getComponent<IdentityComponent>(entityId);
    if (identity) {
        result += ",\"identity\":" + toJson(*identity);
    }

    auto* stats = reg.getComponent<StatsComponent>(entityId);
    if (stats) {
        result += ",\"stats\":" + toJson(*stats);
    }

    auto* personality = reg.getComponent<PersonalityComponent>(entityId);
    if (personality) {
        result += ",\"personality\":" + toJson(*personality);
    }

    auto* lifecycle = reg.getComponent<LifecycleComponent>(entityId);
    if (lifecycle) {
        result += ",\"lifecycle\":" + toJson(*lifecycle);
    }

    auto* social = reg.getComponent<SocialComponent>(entityId);
    if (social) {
        result += ",\"social\":" + toJson(*social);
    }

    auto* memory = reg.getComponent<MemoryRingComponent>(entityId);
    if (memory) {
        // Summary only — report buffer occupancy, not full ring buffer contents
        result += ",\"memory\":{";
        result += "\"hasData\":";
        result += (memory->interactions.size() > 0 || memory->witnessed.size() > 0 ||
                   memory->commandMemory.size() > 0 || memory->rumors.size() > 0 ||
                   memory->midTerm.size() > 0 || memory->longTerm.size() > 0)
                  ? "true" : "false";
        result += ",\"interactions\":" + std::to_string(memory->interactions.size());
        result += ",\"witnessed\":" + std::to_string(memory->witnessed.size());
        result += ",\"commandMemory\":" + std::to_string(memory->commandMemory.size());
        result += ",\"rumors\":" + std::to_string(memory->rumors.size());
        result += ",\"midTerm\":" + std::to_string(memory->midTerm.size());
        result += ",\"longTerm\":" + std::to_string(memory->longTerm.size());
        result += "}";
    }

    auto* skillTree = reg.getComponent<SkillTreeComponent>(entityId);
    if (skillTree) {
        result += ",\"skillTree\":" + toJson(*skillTree);
    }

    auto* career = reg.getComponent<CareerComponent>(entityId);
    if (career) {
        result += ",\"career\":" + toJson(*career);
    }

    auto* evolution = reg.getComponent<EvolutionComponent>(entityId);
    if (evolution) {
        result += ",\"evolution\":" + toJson(*evolution);
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
        if (method == Method::getSchemas)   return handleGetSchemas();
        if (method == Method::getSchema)    return handleGetSchema(raw);
        if (method == Method::describeEntity) return handleDescribeEntity(raw);
        if (method == Method::validateEntity) return handleValidateEntity(raw);
        if (method == Method::listArchetypes) return handleListArchetypes();
        if (method == Method::createFromArchetype) return handleCreateFromArchetype(raw);
        if (method == Method::agentDecide)        return handleAgentDecide(raw);
        if (method == Method::agentTick)          return handleAgentTick(raw);
        if (method == Method::runSimulation)      return handleRunSimulation(raw);

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
        reg.addComponent<StatsComponent>(eid);
        reg.addComponent<PersonalityComponent>(eid);
        reg.addComponent<LifecycleComponent>(eid);
        reg.addComponent<SocialComponent>(eid);
        reg.addComponent<MemoryRingComponent>(eid);
        reg.addComponent<SkillTreeComponent>(eid);
        reg.addComponent<CareerComponent>(eid);
        reg.addComponent<EvolutionComponent>(eid);

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

    // getSchemas: { "method":"getSchemas" }
    // Returns all registered component schemas as JSON Schema
    std::string handleGetSchemas() {
        auto& schemaReg = ECS::SchemaRegistry::instance();
        std::string schemas = json::compact(schemaReg.exportAllJsonSchemas());
        return json::ok("{\"schemas\":" + schemas + "}");
    }

    // getSchema: { "method":"getSchema", "params": { "componentName": "StatsComponent" } }
    // Returns one component's schema
    std::string handleGetSchema(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        std::string componentName = json::getString(params, "componentName");
        if (componentName.empty()) return json::error("componentName is required");

        auto& schemaReg = ECS::SchemaRegistry::instance();
        const ECS::ComponentSchema* schema = schemaReg.getSchema(componentName);
        if (!schema) {
            return json::error("schema not found: " + componentName);
        }

        return json::ok("{\"componentName\":\"" + json::escape(componentName) +
                         "\",\"schema\":" + json::compact(schema->toJsonSchema()) + "}");
    }

    // describeEntity: { "method":"describeEntity", "params": { "entityId": 0 } }
    // Returns what components an entity has + their current values
    std::string handleDescribeEntity(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        auto& schemaReg = ECS::SchemaRegistry::instance();
        std::string result = "{\"entityId\":" + std::to_string(entityId) + ",\"components\":{";
        bool first = true;

        // Helper lambda to append a component
        auto appendComponent = [&](const std::string& name, const std::string& jsonVal) {
            if (!first) result += ",";
            first = false;
            result += "\"" + json::escape(name) + "\":" + jsonVal;
        };

        // Check each of the 9 component types
        {
            auto* c = reg.getComponent<IdentityComponent>(entityId);
            if (c) appendComponent("IdentityComponent", toJson(*c));
        }
        {
            auto* c = reg.getComponent<StatsComponent>(entityId);
            if (c) {
                const ECS::ComponentSchema* schema = schemaReg.getSchema("StatsComponent");
                if (schema && !schema->fields.empty()) {
                    appendComponent("StatsComponent", schema->instanceToJson(c));
                } else {
                    appendComponent("StatsComponent", toJson(*c));
                }
            }
        }
        {
            auto* c = reg.getComponent<PersonalityComponent>(entityId);
            if (c) {
                const ECS::ComponentSchema* schema = schemaReg.getSchema("PersonalityComponent");
                if (schema && !schema->fields.empty()) {
                    appendComponent("PersonalityComponent", schema->instanceToJson(c));
                } else {
                    appendComponent("PersonalityComponent", toJson(*c));
                }
            }
        }
        {
            auto* c = reg.getComponent<LifecycleComponent>(entityId);
            if (c) {
                const ECS::ComponentSchema* schema = schemaReg.getSchema("LifecycleComponent");
                if (schema && !schema->fields.empty()) {
                    appendComponent("LifecycleComponent", schema->instanceToJson(c));
                } else {
                    appendComponent("LifecycleComponent", toJson(*c));
                }
            }
        }
        {
            auto* c = reg.getComponent<SocialComponent>(entityId);
            if (c) {
                const ECS::ComponentSchema* schema = schemaReg.getSchema("SocialComponent");
                if (schema && !schema->fields.empty()) {
                    appendComponent("SocialComponent", schema->instanceToJson(c));
                } else {
                    appendComponent("SocialComponent", toJson(*c));
                }
            }
        }
        {
            auto* c = reg.getComponent<MemoryRingComponent>(entityId);
            if (c) {
                // MemoryRingComponent has no field-level schema, use summary
                std::string memJson = "{\"hasData\":";
                memJson += (c->interactions.size() > 0 || c->witnessed.size() > 0 ||
                            c->commandMemory.size() > 0 || c->rumors.size() > 0 ||
                            c->midTerm.size() > 0 || c->longTerm.size() > 0)
                           ? "true" : "false";
                memJson += ",\"interactions\":" + std::to_string(c->interactions.size());
                memJson += ",\"witnessed\":" + std::to_string(c->witnessed.size());
                memJson += ",\"rumors\":" + std::to_string(c->rumors.size());
                memJson += ",\"longTerm\":" + std::to_string(c->longTerm.size());
                memJson += "}";
                appendComponent("MemoryRingComponent", memJson);
            }
        }
        {
            auto* c = reg.getComponent<SkillTreeComponent>(entityId);
            if (c) appendComponent("SkillTreeComponent", toJson(*c));
        }
        {
            auto* c = reg.getComponent<CareerComponent>(entityId);
            if (c) {
                const ECS::ComponentSchema* schema = schemaReg.getSchema("CareerComponent");
                if (schema && !schema->fields.empty()) {
                    appendComponent("CareerComponent", schema->instanceToJson(c));
                } else {
                    appendComponent("CareerComponent", toJson(*c));
                }
            }
        }
        {
            auto* c = reg.getComponent<EvolutionComponent>(entityId);
            if (c) {
                const ECS::ComponentSchema* schema = schemaReg.getSchema("EvolutionComponent");
                if (schema && !schema->fields.empty()) {
                    appendComponent("EvolutionComponent", schema->instanceToJson(c));
                } else {
                    appendComponent("EvolutionComponent", toJson(*c));
                }
            }
        }

        result += "}}";
        return json::ok(result);
    }

    // validateEntity: { "method":"validateEntity", "params": { "entityId": 0 } }
    // Validates all components on an entity against their schema constraints
    std::string handleValidateEntity(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        ECS::ValidationResult vr = reg.validateEntity(entityId);
        return json::ok(vr.toJson());
    }

    // listArchetypes: { "method":"listArchetypes" }
    // Returns all archetype names, descriptions, and component lists
    std::string handleListArchetypes() {
        // Ensure builtin archetypes are registered
        registerBuiltinArchetypes();

        auto& archReg = ECS::ArchetypeRegistry::instance();
        auto names = archReg.getAllArchetypeNames();

        std::string result = "{\"archetypes\":[";
        bool first = true;
        for (const auto& name : names) {
            const ECS::EntityArchetype* arch = archReg.getArchetype(name);
            if (!arch) continue;

            if (!first) result += ",";
            first = false;

            result += "{\"name\":\"" + json::escape(arch->name) + "\"";
            result += ",\"description\":\"" + json::escape(arch->description) + "\"";
            result += ",\"components\":[";
            for (size_t i = 0; i < arch->components.size(); ++i) {
                if (i > 0) result += ",";
                result += "\"" + json::escape(arch->components[i].componentName) + "\"";
            }
            result += "]}";
        }
        result += "]}";
        return json::ok(result);
    }

    // createFromArchetype: { "method":"createFromArchetype", "params": { "archetype": "Engineer", "name": "小明", "department": "engineering" } }
    // Creates an entity from an archetype template, applies identity overrides, returns full JSON
    std::string handleCreateFromArchetype(const std::string& raw) {
        // Ensure builtin archetypes are registered
        registerBuiltinArchetypes();

        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        std::string archetypeName = json::getString(params, "archetype");
        if (archetypeName.empty()) return json::error("archetype is required");

        auto& archReg = ECS::ArchetypeRegistry::instance();
        const ECS::EntityArchetype* arch = archReg.getArchetype(archetypeName);
        if (!arch) {
            return json::error("unknown archetype: " + archetypeName);
        }

        auto& reg = ECS::Registry::getInstance();
        ECS::EntityId eid;
        try {
            eid = archReg.createFromArchetype(archetypeName, reg);
        } catch (const std::exception& e) {
            return json::error(e.what());
        }

        // Apply identity overrides if provided
        std::string name = json::getString(params, "name");
        std::string department = json::getString(params, "department");

        auto* identity = reg.getComponent<IdentityComponent>(eid);
        if (identity) {
            if (!name.empty()) identity->name = name;
            if (!department.empty()) identity->department = department;
        }

        return json::ok(agentToJson(eid));
    }

    // agentDecide: { "method":"agentDecide", "params": { "entityId": 0, "task": "review this code" } }
    std::string handleAgentDecide(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));
        std::string task = json::getString(params, "task");
        if (task.empty()) return json::error("task is required");

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        // Create LLM client (stub mode if no API key in environment)
        LLM::LLMConfig cfg;
        cfg.provider  = LLM::Provider::Custom;
        cfg.baseUrl   = "http://localhost:8080/v1";
        cfg.model     = "stub-model";
        cfg.maxTokens = 512;
        LLM::LLMClient llmClient(cfg);
        LLM::DecisionEngine engine(&llmClient);

        LLM::Decision decision = engine.decide(reg, entityId, task);
        return json::ok(json::compact(decision.toJson()));
    }

    // agentTick: { "method":"agentTick", "params": { "entityId": 0, "task": "implement feature" } }
    std::string handleAgentTick(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        uint64_t entityId = static_cast<uint64_t>(json::getInt(params, "entityId", -1));
        std::string task = json::getString(params, "task");
        if (task.empty()) return json::error("task is required");

        auto& reg = ECS::Registry::getInstance();
        if (!reg.isEntityValid(entityId)) {
            return json::error("entity not found");
        }

        LLM::LLMConfig cfg;
        cfg.provider  = LLM::Provider::Custom;
        cfg.baseUrl   = "http://localhost:8080/v1";
        cfg.model     = "stub-model";
        cfg.maxTokens = 512;
        if (!tickLlmClient_) {
            tickLlmClient_ = std::make_unique<LLM::LLMClient>(cfg);
            tickEngine_ = std::make_unique<Systems::TickEngine>(tickLlmClient_.get());
        }

        Systems::TickResult result = tickEngine_->tick(reg, entityId, task);
        return json::ok(json::compact(result.toJson()));
    }

    // runSimulation: { "method":"runSimulation", "params": { "entityIds": [0,1], "ticks": 3, "tasks": ["task A","task B"] } }
    std::string handleRunSimulation(const std::string& raw) {
        std::string params = json::getRawValue(raw, "params");
        if (params.empty()) return json::error("missing params");

        int ticks = json::getInt(params, "ticks", 1);
        if (ticks < 1) return json::error("ticks must be >= 1");

        // Parse entityIds array
        std::string idsRaw = json::getRawValue(params, "entityIds");
        std::vector<ECS::EntityId> entityIds;
        {
            size_t pos = 0;
            while ((pos = idsRaw.find_first_of("0123456789", pos)) != std::string::npos) {
                size_t end = idsRaw.find_first_not_of("0123456789", pos);
                entityIds.push_back(static_cast<ECS::EntityId>(std::stoull(idsRaw.substr(pos, end - pos))));
                pos = end;
            }
        }
        if (entityIds.empty()) return json::error("entityIds is empty");

        auto& reg = ECS::Registry::getInstance();
        for (auto id : entityIds) {
            if (!reg.isEntityValid(id)) {
                return json::error("entity not found: " + std::to_string(id));
            }
        }

        // Parse tasks array
        std::vector<std::string> tasks;
        std::string tasksRaw = json::getRawValue(params, "tasks");
        {
            size_t pos = 0;
            while ((pos = tasksRaw.find('"', pos)) != std::string::npos) {
                size_t end = tasksRaw.find('"', pos + 1);
                if (end == std::string::npos) break;
                tasks.push_back(tasksRaw.substr(pos + 1, end - pos - 1));
                pos = end + 1;
            }
        }

        if (!tickLlmClient_) {
            LLM::LLMConfig cfg;
            cfg.provider  = LLM::Provider::Custom;
            cfg.baseUrl   = "http://localhost:8080/v1";
            cfg.model     = "stub-model";
            cfg.maxTokens = 512;
            tickLlmClient_ = std::make_unique<LLM::LLMClient>(cfg);
            tickEngine_ = std::make_unique<Systems::TickEngine>(tickLlmClient_.get());
        }

        Systems::SimulationRunner runner(tickEngine_.get());
        auto results = runner.run(reg, entityIds, ticks, tasks);
        auto summary = Systems::SimulationRunner::summarize(results);

        // Build JSON response
        std::string resp = "{\"results\":[";
        for (size_t i = 0; i < results.size(); ++i) {
            if (i > 0) resp += ",";
            resp += json::compact(results[i].toJson());
        }
        resp += "],\"summary\":" + json::compact(summary.toJson()) + "}";
        return json::ok(resp);
    }

    std::unique_ptr<LLM::LLMClient> tickLlmClient_;
    std::unique_ptr<Systems::TickEngine> tickEngine_;

    UnixSocketServer server_;
};

} // namespace IPC
