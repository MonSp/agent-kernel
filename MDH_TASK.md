---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
branch: improvement/mdh-round1
---

# MDH Task: agent-kernel Multi-Round Improvement

## Report

**What was built** — MDH analyzed agent-kernel source code and executed 3 rounds of improvements:

**Round 1 — DecisionEngine optimization:**
- `actionFromString`: sequential if-chain (O(n)) → `std::unordered_map` lookup (O(1))
- `Decision::toJson`: ostringstream with per-char escaping → pre-allocated `std::string` + shared `appendJsonEscaped` utility
- Eliminated 3 duplicate JSON escaping implementations

**Round 3 — HttpClient hardening:**
- Added `CURLOPT_TCP_KEEPALIVE` for connection reuse (reduces latency for repeated LLM calls)
- Added `CURLOPT_NOSIGNAL` for thread safety in multi-threaded servers
- Added `CURLOPT_FOLLOWLOCATION` + `CURLOPT_MAXREDIRS` for redirect handling
- Added request/response size limits (10 MB) with proper error codes (413/502)

**Round 4 — TickEngine serialization:**
- `TickResult::toJson`: ostringstream → pre-allocated `std::string` with capacity hints
- Numeric fields use `std::to_string` instead of stream insertion
- Pre-reserve based on effects count to minimize allocations

**Verification** — Build passes, 0 new test failures (17 pre-existing IPC tests require running daemon).

## [S1] Problem
agent-kernel had code quality and performance issues identified by MDH's RSI analysis pipeline.

## [S2] Design
See Report section above for each round's changes.

## [S3] Out of Scope
- External dependencies (nlohmann/json)
- Architecture changes
- IPC protocol changes

## Tasks
- [x] T1: DecisionEngine unordered_map enum mapping
- [x] T2: Shared JSON escape utility
- [x] T3: DecisionEngine parser robustness (included in T1 rewrite)
- [x] T4: HttpClient connection reuse + limits
- [x] T5: TickEngine serialization optimization
