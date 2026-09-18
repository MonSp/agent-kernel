---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
---

# MDH Task: agent-kernel Multi-Round Improvement

## Report

### Segfault fix
Root cause: CMake Release build adds `-DNDEBUG`, which compiles out `assert()`.
Tests continued executing after failed assertions, eventually crashing on
invalid state (e.g., sending on fd=-1 after failed socket connect).

Fix:
- `CMakeLists.txt`: `target_compile_options(kernel_tests PRIVATE -UNDEBUG)`
- `test_ipc.cpp`: `assertContains` now calls `abort()` instead of `assert(false)`

### Test coverage additions

**JsonUtils (32 tests):** escape, findString, findInt, findFloat, extractObject,
round-trip, edge cases.

**HttpClient unit (10 tests):** mock response custom/reset/empty, HttpResponse.ok()
boundary (200/299/300/404/500/0), empty body, headers, large body, timeout,
sequential requests.

**SimulationRunner unit (10 tests):** empty entity list, zero ticks, empty tasks,
tick numbering, summary statistics, action counts, confidence range, JSON output,
low energy agent, 10-entity batch.

### Full test results
261 tests passed, 0 failed, exit code 0.
Modules covered: ECS core, components, IPC bridge, schema, dynamic store,
hybrid registry, archetype, validation, LLM, PromptBuilder, DecisionEngine,
action types/effects/executor, TickEngine, SimulationRunner, IPC tick,
EventJournal, AgentMailbox, JsonUtils, HttpClient.
