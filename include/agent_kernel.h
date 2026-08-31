#pragma once

// agent-kernel: Reusable C++ ECS engine for MDH-Company and MDH-Game
//
// Core ECS layer:
//   - Registry  — entity management (slot-based, 100K+ entity capacity)
//   - Entity    — lightweight handle (uint64_t)
//   - Component — CRTP base class with auto TypeId
//   - Archetype — component combination signature
//   - EventStringPool — fixed-size event string pool
//
// Generic components (shared across Company and Game):
//   - IdentityComponent   — ID/name/department/role
//   - StatsComponent      — attributes (HP/MP/power/XP/realm)
//   - PersonalityComponent — 6-dimension personality model
//   - MemoryRingComponent — 3-tier memory (short/mid/long + rumors)
//   - LifecycleComponent  — lifecycle state machine
//   - SocialComponent     — social needs + emotion system
//   - SkillTreeComponent  — skill tree with XP/level-up/dependencies
//   - CareerComponent     — career XP/stage/promotion path
//   - EvolutionComponent  — evolution history/rule tracking/self-evolution

#include "ecs/Component.h"
#include "ecs/Entity.h"
#include "ecs/Archetype.h"
#include "ecs/Registry.h"
#include "ecs/EventStringPool.h"

#include "ecs/components/IdentityComponent.h"
#include "ecs/components/StatsComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include "ecs/components/LifecycleComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/EvolutionComponent.h"
