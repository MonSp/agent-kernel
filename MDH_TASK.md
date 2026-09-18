---
feature: agent-kernel-improvement
status: delivered
updated: 2026-09-16
---

# MDH Task: agent-kernel Coverage Detail Tests

## Coverage Results

| Module | Before | After | Change |
|--------|--------|-------|--------|
| SocialComponent.h | 64% | 77% | +13% |
| EntityArchetype.h | 66% | 73% | +7% |
| SchemaValidator.h | 73% | 73% | — (different compilation units) |
| **TOTAL** | **83.4%** | **83.9%** | **+0.5%** |

## Tests Added (13)

**EntityArchetype applyDefaults type branches (6):**
- Int32 (hp/maxHp/power/xp), Uint8 (careerLevel), Float (ambition/caution)
- Uint32 (totalXp), Int64 (birthTime), Mixed types in one archetype

**SocialComponent cooldown system (3):**
- addCooldown fill all slots
- Reuse expired cooldown slot
- LRU eviction when full

**SchemaValidator field-type validation (4):**
- Float min/max violations (PersonalityComponent)
- Int32 min/max violations (StatsComponent)
- Valid float values pass
- Multiple simultaneous violations (4 fields)

## CI Status
CI passed (4a432b8): coverage gate 80% met at 83.9%.

## Full Test Results
299 tests passed, 0 failed.
