---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
---

# MDH Task: agent-kernel Multi-Round Improvement

## Report

### Round 1 (fe4101b) — Initial improvements
- DecisionEngine: unordered_map enum lookup, shared JSON escape
- HttpClient: size limits, NOSIGNAL thread safety
- TickEngine: pre-allocated string serialization

### Round 2 (f7ed74b) — Review feedback fixes
- Restored deleted comments
- Removed ineffective TCP_KEEPALIVE (no connection pool)
- Removed FOLLOWLOCATION (unnecessary SSRF surface)
- Fixed TickEngine double toJson() call

### Round 3 (this commit) — Deduplication + consistency

**Problem:** JSON helper functions were duplicated across 3 files with
inconsistent behavior:
- `DecisionEngine.cpp`: `appendJsonEscaped` — missing `\b`, `\f`, `\uXXXX`
- `LLMClient.cpp`: `jsonEscape` — had all escape sequences
- `TickEngine.cpp`: `escapeJsonStr` — missing `\b`, `\f`, `\uXXXX`

**Fix:** Created `src/llm/JsonUtils.h` as single source of truth:
- `appendEscaped()` / `escape()` — full escape incl. `\b`, `\f`, `\uXXXX`
- `findString()` / `findInt()` / `findFloat()` — JSON value extraction
- `extractObject()` — balanced-brace JSON object extraction

**Updated files:**
- `DecisionEngine.cpp`: uses JsonUtils (removed 3 duplicate functions)
- `LLMClient.cpp`: uses JsonUtils (removed 2 duplicate functions), `buildRequestBody` now uses `std::string` + `reserve` instead of `ostringstream`
- `TickEngine.cpp`: uses JsonUtils (removed duplicate escape function)

**Net effect:** 3 inconsistent escape implementations → 1 correct one.
LLMClient's superior version (with `\b`/`\f`/`\uXXXX`) is now canonical.

## Tasks
- [x] T1: Create shared JsonUtils.h
- [x] T2: Migrate DecisionEngine to JsonUtils
- [x] T3: Migrate LLMClient to JsonUtils + fix serialization
- [x] T4: Migrate TickEngine to JsonUtils
