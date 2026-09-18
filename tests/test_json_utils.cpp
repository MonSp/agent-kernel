// Tests for JsonUtils — shared JSON helpers for agent-kernel.
#include "llm/JsonUtils.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <cmath>

using namespace JsonUtils;

// ─── appendEscaped / escape ──────────────────────────────────────────────────

static void testEscapeQuotes() {
    assert(escape("say \"hello\"") == "say \\\"hello\\\"");
    printf("  PASS: testEscapeQuotes\n");
}

static void testEscapeBackslash() {
    assert(escape("path\\to\\file") == "path\\\\to\\\\file");
    printf("  PASS: testEscapeBackslash\n");
}

static void testEscapeControlChars() {
    assert(escape("line1\nline2") == "line1\\nline2");
    assert(escape("tab\there") == "tab\\there");
    assert(escape("cr\r") == "cr\\r");
    assert(escape("back\bspace") == "back\\bspace");
    assert(escape("form\ffeed") == "form\\ffeed");
    printf("  PASS: testEscapeControlChars\n");
}

static void testEscapeLowControlChars() {
    // \x01 → \u0001
    std::string input = "a";
    input += '\x01';
    input += "b";
    std::string result = escape(input);
    assert(result == "a\\u0001b");
    printf("  PASS: testEscapeLowControlChars\n");
}

static void testEscapeEmpty() {
    assert(escape("") == "");
    printf("  PASS: testEscapeEmpty\n");
}

static void testEscapeNormalText() {
    assert(escape("hello world") == "hello world");
    assert(escape("no special chars 123") == "no special chars 123");
    printf("  PASS: testEscapeNormalText\n");
}

static void testAppendEscapedIncremental() {
    std::string out = "prefix:";
    appendEscaped(out, "val\"ue");
    assert(out == "prefix:val\\\"ue");
    printf("  PASS: testAppendEscapedIncremental\n");
}

// ─── findString ──────────────────────────────────────────────────────────────

static void testFindStringBasic() {
    assert(findString(R"({"name":"Alice"})", "name") == "Alice");
    printf("  PASS: testFindStringBasic\n");
}

static void testFindStringMissingKey() {
    assert(findString(R"({"name":"Alice"})", "age") == "");
    printf("  PASS: testFindStringMissingKey\n");
}

static void testFindStringEmptyValue() {
    assert(findString(R"({"name":""})", "name") == "");
    printf("  PASS: testFindStringEmptyValue\n");
}

static void testFindStringWithEscapes() {
    assert(findString(R"({"msg":"line1\nline2"})", "msg") == "line1\nline2");
    assert(findString(R"({"msg":"say \"hi\""})", "msg") == "say \"hi\"");
    assert(findString(R"({"msg":"tab\there"})", "msg") == "tab\there");
    printf("  PASS: testFindStringWithEscapes\n");
}

static void testFindStringWithWhitespace() {
    assert(findString(R"({"name" : "Alice"})", "name") == "Alice");
    assert(findString("{\n  \"name\": \"Bob\"\n}", "name") == "Bob");
    printf("  PASS: testFindStringWithWhitespace\n");
}

static void testFindStringNestedKey() {
    // Should find the key regardless of nesting depth
    std::string json = R"({"outer":{"inner":"value"}})";
    assert(findString(json, "inner") == "value");
    printf("  PASS: testFindStringNestedKey\n");
}

static void testFindStringNonStringValue() {
    // Key exists but value is not a string
    assert(findString(R"({"count":42})", "count") == "");
    printf("  PASS: testFindStringNonStringValue\n");
}

// ─── findInt ─────────────────────────────────────────────────────────────────

static void testFindIntBasic() {
    assert(findInt(R"({"count":42})", "count") == 42);
    assert(findInt(R"({"count":0})", "count") == 0);
    assert(findInt(R"({"count":-17})", "count") == -17);
    printf("  PASS: testFindIntBasic\n");
}

static void testFindIntMissingKey() {
    assert(findInt(R"({"count":42})", "missing", 99) == 99);
    printf("  PASS: testFindIntMissingKey\n");
}

static void testFindIntNonNumeric() {
    assert(findInt(R"({"count":"abc"})", "count", -1) == -1);
    printf("  PASS: testFindIntNonNumeric\n");
}

static void testFindIntWithWhitespace() {
    assert(findInt(R"({"count" : 42})", "count") == 42);
    printf("  PASS: testFindIntWithWhitespace\n");
}

// ─── findFloat ───────────────────────────────────────────────────────────────

static void testFindFloatBasic() {
    float v = findFloat(R"({"confidence":0.85})", "confidence", 0.0f);
    assert(std::fabs(v - 0.85f) < 0.001f);
    printf("  PASS: testFindFloatBasic\n");
}

static void testFindFloatNegative() {
    float v = findFloat(R"({"delta":-2.5})", "delta", 0.0f);
    assert(std::fabs(v - (-2.5f)) < 0.001f);
    printf("  PASS: testFindFloatNegative\n");
}

static void testFindFloatInteger() {
    float v = findFloat(R"({"score":3})", "score", 0.0f);
    assert(std::fabs(v - 3.0f) < 0.001f);
    printf("  PASS: testFindFloatInteger\n");
}

static void testFindFloatMissingKey() {
    float v = findFloat(R"({"x":1.0})", "y", 0.5f);
    assert(std::fabs(v - 0.5f) < 0.001f);
    printf("  PASS: testFindFloatMissingKey\n");
}

static void testFindFloatNonNumeric() {
    float v = findFloat(R"({"x":"abc"})", "x", -1.0f);
    assert(std::fabs(v - (-1.0f)) < 0.001f);
    printf("  PASS: testFindFloatNonNumeric\n");
}

// ─── extractObject ───────────────────────────────────────────────────────────

static void testExtractObjectSimple() {
    std::string result = extractObject(R"(prefix {"a":1} suffix)");
    assert(result == R"({"a":1})");
    printf("  PASS: testExtractObjectSimple\n");
}

static void testExtractObjectNested() {
    std::string json = R"({"outer":{"inner":1},"x":2})";
    std::string result = extractObject(json);
    assert(result == json);  // Full balanced object
    printf("  PASS: testExtractObjectNested\n");
}

static void testExtractObjectNoBraces() {
    assert(extractObject("no braces here") == "");
    printf("  PASS: testExtractObjectNoBraces\n");
}

static void testExtractObjectUnmatched() {
    assert(extractObject("{\"a\":1") == "");  // missing closing brace
    printf("  PASS: testExtractObjectUnmatched\n");
}

static void testExtractObjectMultipleObjects() {
    // Should return the first complete object
    std::string result = extractObject(R"({"a":1} and {"b":2})");
    assert(result == R"({"a":1})");
    printf("  PASS: testExtractObjectMultipleObjects\n");
}

static void testExtractObjectEmpty() {
    assert(extractObject("{}") == "{}");
    printf("  PASS: testExtractObjectEmpty\n");
}

// ─── Round-trip: escape → findString ────────────────────────────────────────

static void testRoundTripEscapeParse() {
    // Build JSON with escaped value, then parse it back
    std::string original = "He said \"hello\" and left\n\tend";
    std::string json = "{\"val\":\"" + escape(original) + "\"}";
    std::string parsed = findString(json, "val");
    assert(parsed == original);
    printf("  PASS: testRoundTripEscapeParse\n");
}

static void testRoundTripUnicode() {
    std::string original = "你好世界";
    std::string json = "{\"val\":\"" + escape(original) + "\"}";
    std::string parsed = findString(json, "val");
    assert(parsed == original);
    printf("  PASS: testRoundTripUnicode\n");
}

// ─── Edge cases ──────────────────────────────────────────────────────────────

static void testFindStringKeyAtEnd() {
    // Key exists but no value follows
    assert(findString(R"({"name":)", "name") == "");
    printf("  PASS: testFindStringKeyAtEnd\n");
}

static void testFindIntOverflow() {
    // Very large number — should not crash, returns something reasonable
    findInt(R"({"n":99999999999999})", "n");
    printf("  PASS: testFindIntOverflow (no crash)\n");
}

static void testEscapeAllSpecialCharsCombined() {
    std::string input = "\"\\\n\r\t\b\f";
    // " → \"  \ → \\  \n → \n  \r → \r  \t → \t  \b → \b  \f → \f
    std::string expected = "\\\"\\\\\\n\\r\\t\\b\\f";
    assert(escape(input) == expected);
    printf("  PASS: testEscapeAllSpecialCharsCombined\n");
}

// ─── Runner ──────────────────────────────────────────────────────────────────

void runJsonUtilsTests() {
    printf("Running JsonUtils tests...\n");

    // Escaping
    testEscapeQuotes();
    testEscapeBackslash();
    testEscapeControlChars();
    testEscapeLowControlChars();
    testEscapeEmpty();
    testEscapeNormalText();
    testAppendEscapedIncremental();

    // findString
    testFindStringBasic();
    testFindStringMissingKey();
    testFindStringEmptyValue();
    testFindStringWithEscapes();
    testFindStringWithWhitespace();
    testFindStringNestedKey();
    testFindStringNonStringValue();

    // findInt
    testFindIntBasic();
    testFindIntMissingKey();
    testFindIntNonNumeric();
    testFindIntWithWhitespace();

    // findFloat
    testFindFloatBasic();
    testFindFloatNegative();
    testFindFloatInteger();
    testFindFloatMissingKey();
    testFindFloatNonNumeric();

    // extractObject
    testExtractObjectSimple();
    testExtractObjectNested();
    testExtractObjectNoBraces();
    testExtractObjectUnmatched();
    testExtractObjectMultipleObjects();
    testExtractObjectEmpty();

    // Round-trip
    testRoundTripEscapeParse();
    testRoundTripUnicode();

    // Edge cases
    testFindStringKeyAtEnd();
    testFindIntOverflow();
    testEscapeAllSpecialCharsCombined();

    printf("All 32 JsonUtils tests PASSED.\n");
}
