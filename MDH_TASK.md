---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
---

# MDH Task: agent-kernel Multi-Round Improvement

## Report

### Rounds 1-3 — Code quality improvements
- Round 1 (fe4101b): enum O(1) lookup, shared JSON escape, HttpClient hardening
- Round 2 (f7ed74b): Review fixes — restore comments, remove ineffective options
- Round 3 (253ccd8): JsonUtils.h deduplication — 3 inconsistent escape impls → 1

### Round 4 — Unit tests for JsonUtils (new shared module)

**32 tests covering all JsonUtils.h public functions:**

Escaping (7 tests):
- Quotes, backslash, control chars (\n \r \t \b \f)
- Low control chars (\x01 → \u0001)
- Empty string, normal text
- Incremental appendEscaped

findString (7 tests):
- Basic, missing key, empty value
- Escaped values (\n \" \t)
- Whitespace around key/value
- Nested keys, non-string values

findInt (4 tests):
- Basic, negative, missing key, non-numeric, whitespace

findFloat (5 tests):
- Basic, negative, integer, missing key, non-numeric

extractObject (6 tests):
- Simple, nested, no braces, unmatched, multiple objects, empty

Round-trip (2 tests):
- escape → findString preserves original
- Unicode (你好世界) round-trip

Edge cases (3 tests):
- Key at end without value
- Large number (no crash)
- All special chars combined

**Verification:** 32/32 tests pass (standalone run).
Note: kernel_tests binary segfaults on pre-existing IPC tests
(need running daemon) before reaching JsonUtils tests.

## Tasks
- [x] T1: Create test_json_utils.cpp (32 tests)
- [x] T2: Register in CMakeLists.txt + test runner
- [x] T3: Verify all tests pass
