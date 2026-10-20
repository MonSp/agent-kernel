#include "ProgramLibrary.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Program {

namespace {

bool fileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool dirExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool mkdirs(const std::string& path) {
    if (path.empty()) return false;
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size()) {
            std::string d = cur;
            if (!d.empty() && d.back() == '/') d.pop_back();
            if (d.empty() || d == "/") continue;
            if (!dirExists(d)) {
                if (::mkdir(d.c_str(), 0755) != 0 && errno != EEXIST) return false;
            }
        }
    }
    return dirExists(path);
}

std::string unescapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                default: out.push_back(n); break;
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string jsonEscape(const std::string& s) {
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

std::vector<std::string> parseStringArray(const std::string& raw) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] == '"') {
            size_t j = i + 1;
            std::string tok;
            while (j < raw.size()) {
                if (raw[j] == '\\' && j + 1 < raw.size()) {
                    tok.push_back(raw[j]);
                    tok.push_back(raw[j + 1]);
                    j += 2;
                    continue;
                }
                if (raw[j] == '"') break;
                tok.push_back(raw[j]);
                ++j;
            }
            out.push_back(unescapeJsonString(tok));
            i = (j < raw.size()) ? j + 1 : j;
        } else {
            ++i;
        }
    }
    return out;
}

std::string extractJsonString(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return "";
    size_t colon = body.find(':', p + needle.size());
    if (colon == std::string::npos) return "";
    size_t q1 = body.find('"', colon + 1);
    if (q1 == std::string::npos) return "";
    size_t q2 = q1 + 1;
    while (q2 < body.size()) {
        if (body[q2] == '\\') { q2 += 2; continue; }
        if (body[q2] == '"') break;
        ++q2;
    }
    if (q2 >= body.size()) return "";
    return unescapeJsonString(body.substr(q1 + 1, q2 - q1 - 1));
}

std::string extractJsonArrayRaw(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return "";
    size_t colon = body.find(':', p + needle.size());
    if (colon == std::string::npos) return "";
    size_t lb = body.find('[', colon + 1);
    if (lb == std::string::npos) return "";
    int depth = 0;
    size_t end = lb;
    for (; end < body.size(); ++end) {
        if (body[end] == '[') ++depth;
        else if (body[end] == ']') {
            --depth;
            if (depth == 0) break;
        }
    }
    if (end >= body.size()) return "";
    return body.substr(lb, end - lb + 1);
}

std::string nowIso8601() {
    auto t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::time_t parseIso(const std::string& s) {
    if (s.size() < 19) return 0;
    std::tm tm{};
    if (strptime(s.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm) == nullptr) return 0;
    return timegm(&tm);
}

int countKeywordHits(const std::string& task, const std::vector<std::string>& kws) {
    int hits = 0;
    for (const auto& k : kws) {
        if (!k.empty() && task.find(k) != std::string::npos) ++hits;
    }
    return hits;
}

}  // namespace

bool isValidProgramId(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    auto okFirst = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    };
    if (!okFirst(id[0])) return false;
    for (size_t i = 1; i < id.size(); ++i) {
        char c = id[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::string Manifest::toJson() const {
    std::string kw = "[";
    for (size_t i = 0; i < keywords.size(); ++i) {
        if (i) kw += ",";
        kw += "\"" + jsonEscape(keywords[i]) + "\"";
    }
    kw += "]";
    return "{\"programId\":\"" + jsonEscape(programId) + "\""
           ",\"version\":\"" + jsonEscape(version) + "\""
           ",\"description\":\"" + jsonEscape(description) + "\""
           ",\"status\":\"" + jsonEscape(status) + "\""
           ",\"keywords\":" + kw +
           ",\"modelDependency\":\"" + jsonEscape(modelDependency) + "\""
           ",\"lastValidatedModel\":\"" + jsonEscape(lastValidatedModel) + "\""
           ",\"lastValidatedAt\":\"" + jsonEscape(lastValidatedAt) + "\""
           ",\"consecutiveGateFailures\":" + std::to_string(consecutiveGateFailures)
           + "}";
}

ProgramLibrary::ProgramLibrary(std::string baseDir, std::string pythonBin)
    : baseDir_(std::move(baseDir)), pythonBin_(std::move(pythonBin)) {
    mkdirs(baseDir_);
}

std::string ProgramLibrary::packDir(const std::string& programId) const {
    return baseDir_ + "/" + programId;
}

bool ProgramLibrary::writeManifestJson(const std::string& programId,
                                       const Manifest& m) const {
    std::string dir = packDir(programId);
    if (!mkdirs(dir)) return false;
    std::ofstream f(dir + "/manifest.json", std::ios::trunc);
    if (!f) return false;
    f << m.toJson() << "\n";
    return static_cast<bool>(f);
}

std::optional<Manifest> ProgramLibrary::readManifestJson(
    const std::string& programId) const {
    if (!isValidProgramId(programId)) return std::nullopt;
    std::string path = packDir(programId) + "/manifest.json";
    if (!fileExists(path)) return std::nullopt;
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string body = ss.str();
    Manifest m;
    m.programId = extractJsonString(body, "programId");
    if (m.programId.empty()) m.programId = programId;
    m.version = extractJsonString(body, "version");
    m.description = extractJsonString(body, "description");
    m.status = extractJsonString(body, "status");
    if (m.status.empty()) m.status = "candidate";
    m.keywords = parseStringArray(extractJsonArrayRaw(body, "keywords"));
    m.modelDependency = extractJsonString(body, "modelDependency");
    m.lastValidatedModel = extractJsonString(body, "lastValidatedModel");
    m.lastValidatedAt = extractJsonString(body, "lastValidatedAt");
    {
        std::string needle = "\"consecutiveGateFailures\"";
        size_t p = body.find(needle);
        if (p != std::string::npos) {
            size_t colon = body.find(':', p + needle.size());
            if (colon != std::string::npos) {
                try {
                    m.consecutiveGateFailures = std::stoi(body.substr(colon + 1));
                } catch (...) {
                }
            }
        }
    }
    return m;
}

Manifest ProgramLibrary::compile(const std::string& programId,
                                 const std::string& version,
                                 const std::string& description,
                                 const std::vector<std::string>& keywords,
                                 const std::string& mainPy,
                                 const std::string& testPy,
                                 const std::string& modelDependency) {
    if (!isValidProgramId(programId)) {
        throw std::runtime_error("INVALID_ID: " + programId);
    }
    if (mainPy.empty() || mainPy.find("def run(") == std::string::npos) {
        throw std::runtime_error("BAD_REQUEST: mainPy must contain def run(");
    }
    if (testPy.empty()) {
        throw std::runtime_error("BAD_REQUEST: testPy is required");
    }

    std::lock_guard<std::mutex> g(lock_);
    std::string dir = packDir(programId);
    if (fileExists(dir + "/manifest.json")) {
        throw std::runtime_error("ALREADY_EXISTS: " + programId);
    }
    if (!mkdirs(dir + "/tests")) {
        throw std::runtime_error("BAD_REQUEST: cannot create pack dir");
    }
    {
        std::ofstream mf(dir + "/main.py", std::ios::trunc);
        if (!mf) throw std::runtime_error("BAD_REQUEST: cannot write main.py");
        mf << mainPy;
    }
    {
        std::ofstream tf(dir + "/tests/test_main.py", std::ios::trunc);
        if (!tf) throw std::runtime_error("BAD_REQUEST: cannot write tests");
        tf << testPy;
    }
    Manifest m;
    m.programId = programId;
    m.version = version.empty() ? "1.0.0" : version;
    m.description = description;
    m.status = "candidate";
    m.keywords = keywords;
    m.modelDependency = modelDependency;
    if (!writeManifestJson(programId, m)) {
        throw std::runtime_error("BAD_REQUEST: cannot write manifest");
    }
    // lineage append without re-locking (already hold lock) — write inline
    {
        std::ofstream lf(dir + "/lineage.json", std::ios::app);
        if (lf) {
            lf << "{\"event\":\"compiled\",\"at\":\"" << nowIso8601() << "\"}\n";
        }
    }
    return m;
}

std::optional<Manifest> ProgramLibrary::get(const std::string& programId) const {
    std::lock_guard<std::mutex> g(lock_);
    return readManifestJson(programId);
}

std::vector<Manifest> ProgramLibrary::list(const std::string& statusFilter) const {
    std::lock_guard<std::mutex> g(lock_);
    std::vector<Manifest> out;
    DIR* d = ::opendir(baseDir_.c_str());
    if (!d) return out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (!isValidProgramId(name)) continue;
        auto m = readManifestJson(name);
        if (!m) continue;
        if (!statusFilter.empty() && m->status != statusFilter) continue;
        out.push_back(*m);
    }
    ::closedir(d);
    return out;
}

bool ProgramLibrary::isFresh(const Manifest& m, const std::string& model,
                             int maxAgeDays) const {
    if (m.lastValidatedAt.empty()) return false;
    std::time_t ts = parseIso(m.lastValidatedAt);
    if (ts == 0) return false;
    std::time_t now = std::time(nullptr);
    double ageDays = difftime(now, ts) / 86400.0;
    if (maxAgeDays > 0 && ageDays > static_cast<double>(maxAgeDays)) return false;
    if (!model.empty() && !m.lastValidatedModel.empty() &&
        m.lastValidatedModel != model) {
        return false;
    }
    return true;
}

std::optional<Manifest> ProgramLibrary::match(const std::string& task,
                                              const std::string& model,
                                              int maxAgeDays) const {
    auto all = list("promoted");
    const Manifest* best = nullptr;
    int bestHits = 0;
    for (const auto& m : all) {
        if (!isFresh(m, model, maxAgeDays)) continue;
        int hits = countKeywordHits(task, m.keywords);
        if (hits > bestHits) {
            bestHits = hits;
            best = &m;
        }
    }
    if (!best || bestHits == 0) return std::nullopt;
    return *best;
}

bool ProgramLibrary::promote(const std::string& programId,
                             const std::string& validatedModel) {
    std::lock_guard<std::mutex> g(lock_);
    auto m = readManifestJson(programId);
    if (!m) return false;
    if (m->status != "candidate" && m->status != "promoted") return false;
    m->status = "promoted";
    if (!validatedModel.empty()) m->lastValidatedModel = validatedModel;
    m->lastValidatedAt = nowIso8601();
    if (!writeManifestJson(programId, *m)) return false;
    std::ofstream lf(packDir(programId) + "/lineage.json", std::ios::app);
    if (lf) {
        lf << "{\"event\":\"promoted\",\"validatedModel\":\""
           << jsonEscape(m->lastValidatedModel) << "\",\"at\":\""
           << nowIso8601() << "\"}\n";
    }
    return true;
}

bool ProgramLibrary::recordValidation(const std::string& programId,
                                      const std::string& model) {
    std::lock_guard<std::mutex> g(lock_);
    auto m = readManifestJson(programId);
    if (!m) return false;
    m->lastValidatedModel = model;
    m->lastValidatedAt = nowIso8601();
    return writeManifestJson(programId, *m);
}

bool ProgramLibrary::appendLineage(const std::string& programId,
                                   const std::string& eventJson) {
    std::lock_guard<std::mutex> g(lock_);
    if (!readManifestJson(programId)) return false;
    std::ofstream lf(packDir(programId) + "/lineage.json", std::ios::app);
    if (!lf) return false;
    lf << eventJson << "\n";
    return true;
}

RunResult ProgramLibrary::run(const std::string& programId,
                              const std::string& payloadJson,
                              int timeoutMs) const {
    RunResult r;
    if (!isValidProgramId(programId)) {
        r.errorCode = "INVALID_ID";
        return r;
    }
    std::string dir = packDir(programId);
    std::string mainPy = dir + "/main.py";
    if (!fileExists(mainPy)) {
        r.errorCode = "NOT_FOUND";
        return r;
    }
    if (timeoutMs <= 0) timeoutMs = 15000;

    auto t0 = std::chrono::steady_clock::now();

    // pipe: child stdin/stdout
    int inPipe[2], outPipe[2], errPipe[2];
    if (pipe(inPipe) != 0 || pipe(outPipe) != 0 || pipe(errPipe) != 0) {
        r.errorCode = "RUN_FAILED";
        return r;
    }

    pid_t pid = fork();
    if (pid < 0) {
        r.errorCode = "RUN_FAILED";
        return r;
    }
    if (pid == 0) {
        // child
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        close(errPipe[0]); close(errPipe[1]);
        if (chdir(dir.c_str()) != 0) _exit(127);
        // minimal env
        clearenv();
        setenv("PATH", "/usr/bin:/bin", 1);
        setenv("PYTHONPATH", dir.c_str(), 1);
        const char* py = pythonBin_.c_str();
        execlp(py, py, "-I", "main.py", static_cast<char*>(nullptr));
        _exit(127);
    }

    // parent
    close(inPipe[0]);
    close(outPipe[1]);
    close(errPipe[1]);

    // write payload
    std::string payload = payloadJson;
    if (payload.empty() || payload.front() != '{') payload = "{\"task\":\"\"}";
    payload.push_back('\n');
    ssize_t wn = write(inPipe[1], payload.data(), payload.size());
    (void)wn;
    close(inPipe[1]);

    // read with timeout via waitpid WNOHANG loop + nonblocking is complex;
    // use simple poll: set O_NONBLOCK on out/err and loop
    auto setNb = [](int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    };
    setNb(outPipe[0]);
    setNb(errPipe[0]);

    std::string outBuf, errBuf;
    char buf[4096];
    bool timedOut = false;
    int status = 0;
    bool exited = false;

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);

    while (true) {
        // drain pipes
        ssize_t n;
        while ((n = read(outPipe[0], buf, sizeof(buf))) > 0) outBuf.append(buf, n);
        while ((n = read(errPipe[0], buf, sizeof(buf))) > 0) errBuf.append(buf, n);

        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            exited = true;
            // final drain
            while ((n = read(outPipe[0], buf, sizeof(buf))) > 0) outBuf.append(buf, n);
            while ((n = read(errPipe[0], buf, sizeof(buf))) > 0) errBuf.append(buf, n);
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            timedOut = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(2000);
    }

    close(outPipe[0]);
    close(errPipe[0]);

    auto t1 = std::chrono::steady_clock::now();
    r.durationMs =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.timedOut = timedOut;
    r.stdoutText = outBuf;
    r.stderrText = errBuf;
    if (timedOut) {
        r.ok = false;
        r.errorCode = "TIMED_OUT";
        return r;
    }
    if (!exited) {
        r.ok = false;
        r.errorCode = "RUN_FAILED";
        return r;
    }
    r.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    // trim trailing newline for single-line JSON
    while (!r.stdoutText.empty() &&
           (r.stdoutText.back() == '\n' || r.stdoutText.back() == '\r')) {
        r.stdoutText.pop_back();
    }
    if (r.exitCode != 0) {
        r.ok = false;
        r.errorCode = "RUN_FAILED";
        return r;
    }
    if (r.stdoutText.empty() || r.stdoutText.front() != '{') {
        r.ok = false;
        r.errorCode = "BAD_REQUEST";
        return r;
    }
    r.ok = true;
    r.errorCode.clear();
    return r;
}

}  // namespace Program
