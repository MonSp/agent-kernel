#pragma once

#include "ProgramTypes.h"
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Program {

struct RunResult {
    bool ok = false;
    bool timedOut = false;
    int exitCode = -1;
    double durationMs = 0.0;
    std::string stdoutText;  // expected one-line JSON from program
    std::string stderrText;
    std::string errorCode;   // empty if ok
};

// Disk-backed ProgramPack library shared by Company/Game via agent-kernel.
// Layout matches MDH Python ProgramRegistry (manifest.json primary for kernel).
class ProgramLibrary {
public:
    explicit ProgramLibrary(std::string baseDir, std::string pythonBin = "python3");

    const std::string& baseDir() const { return baseDir_; }

    // Assemble a candidate pack from provided sources ("compile").
    // Writes main.py, tests/test_main.py, manifest.json. Fails if id exists.
    Manifest compile(const std::string& programId,
                     const std::string& version,
                     const std::string& description,
                     const std::vector<std::string>& keywords,
                     const std::string& mainPy,
                     const std::string& testPy,
                     const std::string& modelDependency);

    std::optional<Manifest> get(const std::string& programId) const;
    std::vector<Manifest> list(const std::string& statusFilter = "") const;

    // promoted + fresh match against task text keywords.
    std::optional<Manifest> match(const std::string& task,
                                  const std::string& model = "",
                                  int maxAgeDays = 30) const;

    bool promote(const std::string& programId, const std::string& validatedModel);
    bool recordValidation(const std::string& programId, const std::string& model);
    bool appendLineage(const std::string& programId, const std::string& eventJson);

    RunResult run(const std::string& programId,
                  const std::string& payloadJson,
                  int timeoutMs = 15000) const;

    std::string packDir(const std::string& programId) const;

private:
    bool writeManifestJson(const std::string& programId, const Manifest& m) const;
    std::optional<Manifest> readManifestJson(const std::string& programId) const;
    bool isFresh(const Manifest& m, const std::string& model, int maxAgeDays) const;

    std::string baseDir_;
    std::string pythonBin_;
    mutable std::mutex lock_;
};

}  // namespace Program
