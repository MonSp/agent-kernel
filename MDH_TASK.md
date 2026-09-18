---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
---

# MDH Task: agent-kernel Multi-Round Improvement

## Report

### Round 1 (fe4101b) — Initial improvements
- DecisionEngine: unordered_map enum lookup, shared JSON escape utility
- HttpClient: size limits, NOSIGNAL thread safety
- TickEngine: pre-allocated string serialization

### Round 2 (this commit) — Review feedback fixes

**Code review findings addressed:**

1. **Deleted comments restored** — DecisionEngine.cpp: all explanatory comments
   brought back (`// Parse action`, `// Clamp to [0, 1]`, `// unmatched braces`,
   step numbers in `decide()`, etc.)

2. **TCP_KEEPALIVE removed** — The curl handle is created/destroyed per request
   (`curl_easy_init`/`curl_easy_cleanup`), so TCP keepalive parameters have no
   effect. Real connection reuse would require a handle pool (curl_multi or
   persistent handle cache) — that's an architecture change, not a config tweak.

3. **FOLLOWLOCATION removed** — LLM API endpoints don't return redirects.
   Following redirects adds SSRF attack surface with zero benefit.

4. **TickEngine double toJson() call fixed** — `decision.toJson()` was called
   twice (once in `reserve()`, once in concatenation). Now serialized once
   into a local variable.

**What was kept from Round 1:**
- unordered_map enum lookup (real O(1) improvement + maintainability)
- Shared JSON escape utility (fixes missing \n/\r/\t in delegateTo/details)
- Size limits + NOSIGNAL (real safety/threading fixes)
- std::string + reserve serialization (micro-optimization, kept)

## Tasks
- [x] T1: Restore deleted comments
- [x] T2: Remove TCP_KEEPALIVE (ineffective without connection pool)
- [x] T3: Remove FOLLOWLOCATION (unnecessary SSRF surface)
- [x] T4: Fix TickEngine double toJson() call
