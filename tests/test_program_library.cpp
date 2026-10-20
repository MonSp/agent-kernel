// ProgramLibrary unit tests — compile / list / get / promote / match / run / lineage
#include "program/ProgramLibrary.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

using namespace Program;

static std::string makeTempDir() {
    char tmpl[] = "/tmp/proglibXXXXXX";
    char* d = mkdtemp(tmpl);
    assert(d != nullptr);
    return std::string(d);
}

static const char* kMainPy =
    "import json, sys\n"
    "def run(data):\n"
    "    task = data.get('task', '')\n"
    "    return {'ok': True, 'echo': task}\n"
    "if __name__ == '__main__':\n"
    "    json.dump(run(json.load(sys.stdin)), sys.stdout)\n";

static const char* kTestPy =
    "import main\n"
    "def test_run():\n"
    "    assert main.run({'task': 'x'})['ok'] is True\n";

static void test_valid_id() {
    assert(isValidProgramId("abc"));
    assert(isValidProgramId("a_b-c1"));
    assert(!isValidProgramId(""));
    assert(!isValidProgramId("Abc"));
    assert(isValidProgramId("1abc"));  // digits first OK — matches ^[a-z0-9_]
    assert(!isValidProgramId("a b"));
    assert(!isValidProgramId(std::string(65, 'a')));
    std::printf("test_valid_id OK\n");
}

static void test_compile_get_list() {
    std::string dir = makeTempDir();
    ProgramLibrary lib(dir, "python3");
    auto m = lib.compile("echo_task", "1.0.0", "echo task",
                         {"写文件", "echo"}, kMainPy, kTestPy, "deepseek-chat");
    assert(m.programId == "echo_task");
    assert(m.status == "candidate");

    auto got = lib.get("echo_task");
    assert(got.has_value());
    assert(got->version == "1.0.0");
    assert(got->keywords.size() == 2);

    auto all = lib.list("");
    assert(all.size() == 1);
    auto cands = lib.list("candidate");
    assert(cands.size() == 1);
    auto promos = lib.list("promoted");
    assert(promos.empty());

    bool threw = false;
    try {
        lib.compile("echo_task", "1.0.1", "", {}, kMainPy, kTestPy, "");
    } catch (...) {
        threw = true;
    }
    assert(threw);
    std::printf("test_compile_get_list OK\n");
}

static void test_invalid_compile() {
    std::string dir = makeTempDir();
    ProgramLibrary lib(dir, "python3");
    bool threw = false;
    try {
        lib.compile("BadId", "1.0.0", "", {}, kMainPy, kTestPy, "");
    } catch (...) {
        threw = true;
    }
    assert(threw);
    threw = false;
    try {
        lib.compile("ok_id", "1.0.0", "", {}, "no run here", kTestPy, "");
    } catch (...) {
        threw = true;
    }
    assert(threw);
    std::printf("test_invalid_compile OK\n");
}

static void test_promote_match_run() {
    std::string dir = makeTempDir();
    ProgramLibrary lib(dir, "python3");
    lib.compile("writer", "1.0.0", "write helper",
                {"写文件", "out.txt"}, kMainPy, kTestPy, "");
    assert(lib.promote("writer", "deepseek-chat"));

    auto m = lib.match("请写文件：生成 out.txt", "deepseek-chat", 30);
    assert(m.has_value());
    assert(m->programId == "writer");

    // model mismatch → no match
    auto m2 = lib.match("请写文件", "other-model", 30);
    assert(!m2.has_value());

    // no keyword hit
    auto m3 = lib.match("完全无关任务", "deepseek-chat", 30);
    assert(!m3.has_value());

    auto r = lib.run("writer", "{\"task\":\"hello\"}", 10000);
    assert(r.ok);
    assert(r.exitCode == 0);
    assert(r.stdoutText.find("\"ok\"") != std::string::npos);
    assert(r.stdoutText.find("hello") != std::string::npos);

    // missing pack
    auto r2 = lib.run("no_such", "{}", 1000);
    assert(!r2.ok);
    assert(r2.errorCode == "NOT_FOUND");

    assert(lib.recordValidation("writer", "deepseek-chat"));
    assert(lib.appendLineage("writer", "{\"event\":\"tested\"}"));
    std::printf("test_promote_match_run OK\n");
}

static void test_lineage_file() {
    std::string dir = makeTempDir();
    ProgramLibrary lib(dir, "python3");
    lib.compile("lin", "1.0.0", "", {"x"}, kMainPy, kTestPy, "");
    std::string path = lib.packDir("lin") + "/lineage.json";
    std::ifstream f(path);
    assert(f.good());
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    assert(content.find("compiled") != std::string::npos);
    std::printf("test_lineage_file OK\n");
}

void runProgramLibraryTests() {
    std::printf("\n=== ProgramLibrary tests ===\n");
    test_valid_id();
    test_compile_get_list();
    test_invalid_compile();
    test_promote_match_run();
    test_lineage_file();
    std::printf("ALL program library tests passed\n");
}
