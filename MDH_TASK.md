---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
---

# MDH Task: agent-kernel Multi-Round Improvement

## Coverage Report + Gap Fill

### Coverage analysis (gcov, best-per-file)

| Module | Coverage | Gap |
|--------|----------|-----|
| Component.h | 27.3% | Template methods not instantiated |
| EntityArchetype.h | 64.2% | applyDefaults not tested |
| AgentKernelBridge.h | 69.7% | Many IPC endpoints untested |
| EventJournal.h | 70.9% | Edge cases untested |
| SchemaValidator.h | 73.1% | toJson/validate untested |
| Schema.h | 80.9% | Partial |
| JsonUtils.h | 91.1% | Good |
| DecisionEngine.cpp | 96.4% | Good |
| LLMClient.cpp | 95.3% | Good |
| TickEngine.cpp | 97.4% | Good |

### Gap fill: 13 new tests

**Component type IDs (3):**
- generateComponentTypeId unique incrementing
- Component<T>::getStaticTypeId unique per type
- IComponent virtual base

**SchemaValidator (4):**
- ValidationResult.toJson valid/invalid
- ComponentSchema::validate pass/fail
- Violation detection for out-of-range values

**EntityArchetype applyDefaults (3):**
- Default values applied to Stats/Personality
- Numeric field parsing
- Invalid field gracefully ignored

**EventJournal edge cases (3):**
- Ring buffer overflow (200 events)
- Query by type
- Clear and reuse

### Full test results
275 tests passed, 0 failed.
