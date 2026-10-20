#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Program {

enum class Status {
    Candidate,
    Promoted,
    Deprecated,
};

inline std::string statusToString(Status s) {
    switch (s) {
        case Status::Candidate:  return "candidate";
        case Status::Promoted:   return "promoted";
        case Status::Deprecated: return "deprecated";
    }
    return "candidate";
}

inline Status statusFromString(const std::string& s) {
    if (s == "promoted")   return Status::Promoted;
    if (s == "deprecated") return Status::Deprecated;
    return Status::Candidate;
}

struct Manifest {
    std::string programId;
    std::string version;
    std::string description;
    std::string status;  // candidate | promoted | deprecated
    std::vector<std::string> keywords;
    std::string modelDependency;
    std::string lastValidatedModel;
    std::string lastValidatedAt;  // ISO-8601 UTC or empty
    int consecutiveGateFailures = 0;

    std::string toJson() const;
};

// program_id: ^[a-z0-9_][a-z0-9_-]{0,63}$
bool isValidProgramId(const std::string& id);

}  // namespace Program
