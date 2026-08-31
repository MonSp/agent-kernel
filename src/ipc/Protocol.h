#pragma once

#include <string>
#include <cstdint>

namespace IPC {

// --- Message types ---
enum class MessageType : uint8_t {
    Request  = 1,
    Response = 2,
    Push     = 3,
};

// --- Request/Response/Push message envelope ---
// Request:  { "method": "...", "params": { ... } }
// Response: { "ok": true/false, "data": { ... } }  or  { "ok": false, "error": "..." }
// Push:     { "event": "...", ... }

struct Message {
    MessageType type;
    std::string id;       // request correlation id (optional)
    std::string method;   // for requests
    std::string payload;  // raw JSON body
};

// --- Method name constants ---
namespace Method {
    inline const std::string createAgent = "createAgent";
    inline const std::string getAgent    = "getAgent";
    inline const std::string updateAgent = "updateAgent";
    inline const std::string deleteAgent = "deleteAgent";
    inline const std::string listAgents  = "listAgents";
    inline const std::string addSkill     = "addSkill";
    inline const std::string addSkillXp   = "addSkillXp";
    inline const std::string addCareerXp  = "addCareerXp";
    inline const std::string getSkills    = "getSkills";
    inline const std::string syncState    = "syncState";
} // namespace Method

} // namespace IPC
